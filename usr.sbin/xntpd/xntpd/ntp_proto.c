/*
 * ntp_proto.c - NTP version 3 protocol machinery
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"

/*
 * System variables are declared here.  See Section 3.2 of the
 * specification.
 */
u_char	sys_leap;		/* system leap indicator */
u_char	sys_stratum;		/* stratum of system */
s_char	sys_precision;		/* local clock precision */
s_fp	sys_rootdelay;		/* distance to current sync source */
u_fp	sys_rootdispersion;	/* dispersion of system clock */
u_long	sys_refid;		/* reference source for local clock */
l_fp	sys_offset;		/* combined offset from clock_select */
u_fp	sys_maxd;		/* dispersion of selected peer */
l_fp	sys_reftime;		/* time we were last updated */
l_fp	sys_refskew;		/* accumulated skew since last update */
struct	peer *sys_peer;		/* our current peer */
u_char	sys_poll;		/* log2 of system poll interval */
extern	long sys_clock;		/* second part of current time */
long	sys_lastselect;		/* sys_clock at last synch update */

/*
 * Nonspecified system state variables.
 */
int	sys_bclient;		/* we set our time to broadcasts */
s_fp	sys_bdelay;		/* broadcast client default delay */
int	sys_authenticate;	/* authenticate time used for syncing */
u_char	consensus_leap;		/* mitigated leap bits */
u_long	sys_authdelay;		/* encryption time (l_fp fraction) */
u_char	leap_consensus;		/* consensus of survivor leap bits */

/*
 * Statistics counters
 */
u_long	sys_stattime;		/* time when we started recording */
u_long	sys_badstratum;		/* packets with invalid stratum */
u_long	sys_oldversionpkt;	/* old version packets received */
u_long	sys_newversionpkt;	/* new version packets received */
u_long	sys_unknownversion;	/* don't know version packets */
u_long	sys_badlength;		/* packets with bad length */
u_long	sys_processed;		/* packets processed */
u_long	sys_badauth;		/* packets dropped because of auth */
u_long	sys_limitrejected;	/* pkts rejected due toclient count per net */

/*
 * Imported from ntp_timer.c
 */
extern u_long current_time;
extern struct event timerqueue[];

/*
 * Imported from ntp_io.c
 */
extern struct interface *any_interface;

/*
 * Imported from ntp_loopfilter.c
 */
extern int pll_enable;
extern int pps_update;

/*
 * Imported from ntp_util.c
 */
extern int stats_control;

/*
 * The peer hash table. Imported from ntp_peer.c
 */
extern struct peer *peer_hash[];
extern int peer_hash_count[];

/*
 * debug flag
 */
extern int debug;

static	void	clear_all	P((void));

/*
 * transmit - Transmit Procedure. See Section 3.4.1 of the
 *	specification.
 */
void
transmit(peer)
	register struct peer *peer;
{
	struct pkt xpkt;	/* packet to send */
	u_long peer_timer;
	u_fp precision;
	int bool;

	/*
	 * We need to be very careful about honking uncivilized time. If
	 * not operating in broadcast mode, honk in all except broadcast
	 * client mode. If operating in broadcast mode and synchronized
	 * to a real source, honk except when the peer is the local-
	 * clock driver and the prefer flag is not set. In other words,
	 * in broadcast mode we never honk unless known to be
	 * synchronized to real time.
	 */
	bool = 0;
	if (peer->hmode != MODE_BROADCAST) {
		if (peer->hmode != MODE_BCLIENT)
			bool = 1;
	} else if (sys_peer != 0 && sys_leap != LEAP_NOTINSYNC) {
		if (!(sys_peer->refclktype == REFCLK_LOCALCLOCK &&
		    !(sys_peer->flags & FLAG_PREFER)))
			bool = 1;
	}
	if (bool) {
		u_long xkeyid;
		int find_rtt = (peer->cast_flags & MDF_MCAST) &&
		    peer->hmode != MODE_BROADCAST;

		/*
		 * Figure out which keyid to include in the packet
		 */
		if ((peer->flags & FLAG_AUTHENABLE)
		    && (peer->flags & (FLAG_CONFIG|FLAG_AUTHENTIC))
		    && authhavekey(peer->keyid)) {
			xkeyid = peer->keyid;
		} else {
			xkeyid = 0;
		}

		/*
		 * Make up a packet to send.
		 */
		xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
		    peer->version, peer->hmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
		xpkt.ppoll = peer->hpoll;
		xpkt.precision = sys_precision;
		xpkt.rootdelay = HTONS_FP(sys_rootdelay);
		precision = FP_SECOND >> -(int)sys_precision;
		if (precision == 0)
			precision = 1;
		xpkt.rootdispersion = HTONS_FP(sys_rootdispersion +
		    precision + LFPTOFP(&sys_refskew));
		xpkt.refid = sys_refid;
		HTONL_FP(&sys_reftime, &xpkt.reftime);
		HTONL_FP(&peer->org, &xpkt.org);
		HTONL_FP(&peer->rec, &xpkt.rec);

		/*
		 * Decide whether to authenticate or not. If so, call
		 * encrypt() to fill in the rest of the frame. If not,
		 * just add in the xmt timestamp and send it quick.
		 */
		if (peer->flags & FLAG_AUTHENABLE) {
			int sendlen;

			xpkt.keyid = htonl(xkeyid);
			auth1crypt(xkeyid, (U_LONG *)&xpkt,
			    LEN_PKT_NOMAC);
			get_systime(&peer->xmt);
			L_ADDUF(&peer->xmt, sys_authdelay);
			HTONL_FP(&peer->xmt, &xpkt.xmt);
			sendlen = auth2crypt(xkeyid, (U_LONG *)&xpkt,
					     LEN_PKT_NOMAC);
			sendpkt(&peer->srcadr, find_rtt ?
			    any_interface : peer->dstadr,
			    ((peer->cast_flags & MDF_MCAST) && !find_rtt) ?
			    peer->ttl : -7, &xpkt, sendlen +
			    LEN_PKT_NOMAC);
#ifdef DEBUG
			if (debug > 1)
				printf("transmit auth to %s\n",
				    ntoa(&(peer->srcadr)));
#endif
			peer->sent++;
		} else {
			/*
			 * Get xmt timestamp, then send it without mac
			 * field
			 */
			int find_rtt = (peer->cast_flags & MDF_MCAST) &&
			    peer->dstadr != any_interface;
			get_systime(&(peer->xmt));
			HTONL_FP(&peer->xmt, &xpkt.xmt);
			sendpkt(&(peer->srcadr), find_rtt ?
			    any_interface : peer->dstadr,
			    ((peer->cast_flags & MDF_MCAST) && !find_rtt) ?
			    peer->ttl : -8, &xpkt, LEN_PKT_NOMAC);
#ifdef DEBUG
			if (debug > 1)
				printf("transmit to %s\n",
				    ntoa(&(peer->srcadr)));
#endif
			peer->sent++;
		}
	}

	if (peer->hmode != MODE_BROADCAST) {
		u_char opeer_reach;
		/*
		 * Determine reachability and diddle things if we
		 * haven't heard from the host for a while. If we are
		 * about to become unreachable and are a
		 * broadcast/multicast client, the server has refused to
		 * boogie in client/server mode, so we switch to
		 * MODE_BCLIENT anyway and wait for subsequent
		 * broadcasts.
		 */
		opeer_reach = peer->reach;
		if (opeer_reach & 0x80 && peer->flags & FLAG_MCAST2) {
			peer->hmode = MODE_BCLIENT;
		}
		peer->reach <<= 1;
		if (peer->reach == 0) {
			if (opeer_reach != 0)
				report_event(EVNT_UNREACH, peer);
			/*
			 * Clear this guy out.  No need to redo clock
			 * selection since by now this guy won't be a
			 * player
			 */
			if (peer->flags & FLAG_CONFIG) {
				if (opeer_reach != 0) {
					peer_clear(peer);
					peer->timereachable =
					    current_time;
				}
			}

			/*
			 * While we have a chance, if our system peer is
			 * zero or his stratum is greater than the last
			 * known stratum of this guy, make sure hpoll is
			 * clamped to the minimum before resetting the
			 * timer. If the peer has been unreachable for a
			 * while and we have a system peer who is at
			 * least his equal, we may want to ramp his
			 * polling interval up to avoid the useless
			 * traffic.
			 */
			if (sys_peer == 0) {
				peer->hpoll = peer->minpoll;
				peer->unreach = 0;
			} else if (sys_peer->stratum > peer->stratum) {
				peer->hpoll = peer->minpoll;
				peer->unreach = 0;
			} else {
				if (peer->unreach < 16) {
					peer->unreach++;
					peer->hpoll = peer->minpoll;
				} else if (peer->hpoll < peer->maxpoll) {
					peer->hpoll++;
					peer->ppoll = peer->hpoll;
				}
			}

		/*
		 * Update reachability and poll variables
		 */
		} else if ((opeer_reach & 3) == 0) {

			l_fp off;

			if (peer->valid > 0)
				peer->valid--;
			if (peer->hpoll > peer->minpoll)
				peer->hpoll--;
			L_CLR(&off);
			clock_filter(peer, &off, (s_fp)0,
			    (u_fp)NTP_MAXDISPERSE);
			if (peer->flags & FLAG_SYSPEER)
				clock_select();
		} else {
			if (peer->valid < NTP_SHIFT) {
				peer->valid++;
			} else {
				if (peer->hpoll < peer->maxpoll)
					peer->hpoll++;
			}
		}
	}

	/*
	 * Finally, adjust the hpoll variable for special conditions. If
	 * we are a broadcast/multicast client, we use the server poll
	 * interval if listening for broadcasts and one-eighth this
	 * interval if in client/server mode. The following clamp
	 * prevents madness. If this is the system poll, sys_poll
	 * controls hpoll.
	 */
	if (peer->flags & FLAG_MCAST2) {
		if (peer->hmode == MODE_BCLIENT)
			peer->hpoll = peer->ppoll;
		else
			peer->hpoll = peer->ppoll - 3;
	} else if (peer->flags & FLAG_SYSPEER)
		peer->hpoll = sys_poll;
	if (peer->hpoll < peer->minpoll)
		peer->hpoll = peer->minpoll;

	/*
	 * Arrange for our next timeout. hpoll will be less than maxpoll
	 * for sure.
	 */
	if (peer->event_timer.next != 0)
		/*
		 * Oops, someone did already.
		 */
		TIMER_DEQUEUE(&peer->event_timer);
	peer_timer = 1 << (int)max((u_char)min(peer->ppoll,
	    peer->hpoll), peer->minpoll);
	peer->event_timer.event_time = current_time + peer_timer;
	TIMER_ENQUEUE(timerqueue, &peer->event_timer);
}

/*
 * receive - Receive Procedure.  See section 3.4.2 in the specification.
 */
void
receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct peer *peer;
	register struct pkt *pkt;
	register u_char hismode;
	int restrict;
	int has_mac;
	int trustable;
	int is_authentic;
	u_long hiskeyid;
	struct peer *peer2;

#ifdef DEBUG
	if (debug > 1)
		printf("receive from %s\n", ntoa(&rbufp->recv_srcadr));
#endif

	/*
	 * Let the monitoring software take a look at this first.
	 */
	monitor(rbufp);

	/*
	 * Get the restrictions on this guy.  If we're to ignore him,
	 * go no further.
	 */
	restrict = restrictions(&rbufp->recv_srcadr);
	if (restrict & RES_IGNORE)
		return;

	/*
	 * Get a pointer to the packet.
	 */
	pkt = &rbufp->recv_pkt;

	/*
	 * Catch packets whose version number we can't deal with
	 */
	if (PKT_VERSION(pkt->li_vn_mode) >= NTP_VERSION) {
		sys_newversionpkt++;
	} else if (PKT_VERSION(pkt->li_vn_mode) >= NTP_OLDVERSION) {
		sys_oldversionpkt++;
	} else {
		sys_unknownversion++;
		return;
	}

	/*
	 * Catch private mode packets. Dump it if queries not allowed.
	 */
	if (PKT_MODE(pkt->li_vn_mode) == MODE_PRIVATE) {
		if (restrict & RES_NOQUERY)
			return;
		process_private(rbufp, ((restrict&RES_NOMODIFY) == 0));
		return;
	}

	/*
	 * Same with control mode packets.
	 */
	if (PKT_MODE(pkt->li_vn_mode) == MODE_CONTROL) {
		if (restrict & RES_NOQUERY)
			return;
		process_control(rbufp, restrict);
		return;
	}

	/*
	 * See if we're allowed to serve this guy time.  If not, ignore
	 * him.
	 */
	if (restrict & RES_DONTSERVE)
		return;

	/*
	 * See if we only accept limited number of clients from the net
	 * this guy is from. Note: the flag is determined dynamically
	 * within restrictions()
	 */
	if (restrict & RES_LIMITED) {
		extern u_long client_limit;

		sys_limitrejected++;
		syslog(LOG_NOTICE,
	   "rejected mode %d request from %s - per net client limit (%d) exceeded",
		       PKT_MODE(pkt->li_vn_mode),
		       ntoa(&rbufp->recv_srcadr), client_limit);
		return;
	}
	/*
	 * Dump anything with a putrid stratum.  These will most likely
	 * come from someone trying to poll us with ntpdc.
	 */
	if (pkt->stratum > NTP_MAXSTRATUM) {
		sys_badstratum++;
		return;
	}

	/*
	 * Find the peer.  This will return a null if this guy isn't in
	 * the database.
	 */
	peer = findpeer(&rbufp->recv_srcadr, rbufp->dstadr, rbufp->fd);

	/*
	 * Check the length for validity, drop the packet if it is
	 * not as expected. If this is a client mode poll, go no
	 * further. Send back his time and drop it.
	 *
	 * The scheme we use for authentication is this.  If we are
	 * running in non-authenticated mode, we accept both frames
	 * which are authenticated and frames which aren't, but don't
	 * authenticate. We do record whether the frame had a mac field
	 * or not so we know what to do on output.
	 *
	 * If we are running in authenticated mode, we only trust frames
	 * which have authentication attached, which are validated and
	 * which are using one of our trusted keys. We respond to all
	 * other pollers without saving any state. If a host we are
	 * passively peering with changes his key from a trusted one to
	 * an untrusted one, we immediately unpeer with him, reselect
	 * the clock and treat him as an unmemorable client (this is
	 * a small denial-of-service hole I'll have to think about).
	 * If a similar event occurs with a configured peer we drop the
	 * frame and hope he'll revert to our key again. If we get a
	 * frame which can't be authenticated with the given key, we
	 * drop it. Either we disagree on the keys or someone is trying
	 * some funny stuff.
	 */

	/*
	 * here we assume that any packet with an authenticator is at
	 * least LEN_PKT_MAC bytes long, which means at least 96 bits
	 */
	if (rbufp->recv_length >= LEN_PKT_MAC) {
		has_mac = rbufp->recv_length - LEN_PKT_NOMAC;
		hiskeyid = ntohl(pkt->keyid);
#ifdef	DEBUG
		if (debug > 2)
			printf(
	    "receive: pkt is %d octets, mac %d octets long, keyid %ld\n",
			    rbufp->recv_length, has_mac, hiskeyid);
#endif
	} else if (rbufp->recv_length == LEN_PKT_NOMAC) {
		hiskeyid = 0;
		has_mac = 0;
	} else {
#ifdef DEBUG
		if (debug > 2)
			printf("receive: bad length %d %ld\n",
			    rbufp->recv_length, sizeof(struct pkt));
#endif
		sys_badlength++;
		return;
	}


	/*
	 * Figure out his mode and validate it.
	 */
	hismode = PKT_MODE(pkt->li_vn_mode);
#ifdef DEBUG
	if (debug > 2)
		printf("receive: his mode %d\n", hismode);
#endif
	if (PKT_VERSION(pkt->li_vn_mode) == NTP_OLDVERSION && hismode ==
	    0) {
		/*
		 * Easy.  If it is from the NTP port it is
		 * a sym act, else client.
		 */
		if (SRCPORT(&rbufp->recv_srcadr) == NTP_PORT)
			hismode = MODE_ACTIVE;
		else
			hismode = MODE_CLIENT;
	} else {
		if (hismode != MODE_ACTIVE && hismode != MODE_PASSIVE &&
		    hismode != MODE_SERVER && hismode != MODE_CLIENT &&
		    hismode != MODE_BROADCAST) {
			syslog(LOG_ERR, "bad mode %d received from %s",
			    PKT_MODE(pkt->li_vn_mode),
			    ntoa(&rbufp->recv_srcadr));
			return;
		}
	}

	/*
	 * If he included a mac field, decrypt it to see if it is
	 * authentic.
	 */
	is_authentic = 0;
	if (has_mac) {
		if (authhavekey(hiskeyid)) {
			if (!authistrusted(hiskeyid)) {
				sys_badauth++;
#ifdef DEBUG
				if (debug > 3)
			printf("receive: untrusted keyid\n");
#endif
				return;
			}
			if (authdecrypt(hiskeyid, (U_LONG *)pkt,
			    LEN_PKT_NOMAC)) {
				is_authentic = 1;
#ifdef DEBUG
				if (debug > 3)
			printf("receive: authdecrypt succeeds\n");
#endif
			} else {
				sys_badauth++;
#ifdef DEBUG
				if (debug > 3)
			printf("receive: authdecrypt fails\n");
#endif
			}
		}
	}

	/*
	 * If this is someone we don't remember from a previous
	 * association, dispatch him now.  Either we send something back
	 * quick, we ignore him, or we allocate some memory for him and
	 * let him continue.
	 */
	if (peer == 0) {
		int mymode;

		mymode = MODE_PASSIVE;
		switch(hismode) {
		case MODE_ACTIVE:
			/*
			 * See if this guy qualifies as being the least
			 * bit memorable.  If so we keep him around for
			 * later.  If not, send his time quick.
			 */
			if (restrict & RES_NOPEER) {
				fast_xmit(rbufp, (int)hismode,
				    is_authentic);
				return;
			}
			break;

		case MODE_PASSIVE:
		case MODE_SERVER:
			/*
			 * These are obvious errors.  Ignore.
			 */
			return;

		case MODE_CLIENT:
			/*
			 * Send it back quick and go home.
			 */
			fast_xmit(rbufp, (int)hismode, is_authentic);
			return;

		case MODE_BROADCAST:
			/*
			 * Sort of a repeat of the above...
			 */
			if ((restrict & RES_NOPEER) || !sys_bclient)
				return;
			mymode = MODE_MCLIENT;
			break;
		}

		/*
		 * Okay, we're going to keep him around.  Allocate him
		 * some memory.
		 */
		peer = newpeer(&rbufp->recv_srcadr,
		    rbufp->dstadr, mymode, PKT_VERSION(pkt->li_vn_mode),
		    NTP_MINDPOLL, NTP_MAXDPOLL, 0, hiskeyid);

		if (peer == 0) {
			/*
			 * The only way this can happen is if the
			 * source address looks like a reference
			 * clock.  Since this is an illegal address
			 * this is one of those "can't happen" things.
			 */
			syslog(LOG_ERR,
			    "receive() failed to peer with %s, mode %d",
			    ntoa(&rbufp->recv_srcadr), mymode);
			return;
		}
	}

	/*
	 * Mark the time of reception
	 */
	peer->timereceived = current_time;

	/*
	 * If the peer isn't configured, set his keyid and authenable
	 * status based on the packet.
	 */
	if (!(peer->flags & FLAG_CONFIG)) {
		if (has_mac) {
		    if (!(peer->reach && peer->keyid != hiskeyid)) {
			peer->keyid = hiskeyid;
			peer->flags |= FLAG_AUTHENABLE;
		    } 
		} else {
			peer->keyid = 0;
			peer->flags &= ~FLAG_AUTHENABLE;
		}
	}


	/*
	 * If this message was authenticated properly, note this
	 * in the flags.
	 */
	if (is_authentic) {
		peer->flags |= FLAG_AUTHENTIC;
	} else {
		/*
		 * If this guy is authenable, and has been authenticated
		 * in the past, but just failed the authentic test,
		 * report the event.
		 */
		if (peer->flags & FLAG_AUTHENABLE
		    && peer->flags & FLAG_AUTHENTIC)
			report_event(EVNT_PEERAUTH, peer);
		peer->flags &= ~FLAG_AUTHENTIC;
	}

	/*
	 * Determine if this guy is basically trustable.
	 */
	if (restrict & RES_DONTTRUST)
		trustable = 0;
	else
		trustable = 1;
	
	if (sys_authenticate && trustable) {
		if (!(peer->flags & FLAG_CONFIG) ||
		    (peer->flags & FLAG_AUTHENABLE)) {
			if (has_mac && is_authentic)
				trustable = 1;
			else
				trustable = 0;
			}
	}

	/*
	 * Dispose of the packet based on our respective modes. We
	 * don't drive this with a table, though we probably could.
	 */
	switch (peer->hmode) {
	case MODE_ACTIVE:
	case MODE_CLIENT:
		/*
		 * Active mode associations are configured. If the data
		 * isn't trustable, ignore it and hope this guy
		 * brightens up. Else accept any data we get and process
		 * it.
		 */
		switch (hismode) {
		case MODE_ACTIVE:
		case MODE_PASSIVE:
		case MODE_SERVER:
		case MODE_BROADCAST:
			process_packet(peer, pkt, &(rbufp->recv_time),
			    has_mac, trustable);
			break;

		case MODE_CLIENT:
			if (peer->hmode == MODE_ACTIVE)
				fast_xmit(rbufp, hismode, is_authentic);
			return;
		}
		break;

	case MODE_PASSIVE:
		/*
		 * Passive mode associations are (in the current
		 * implementation) always dynamic. If we get an invalid
		 * header, break the connection. I hate doing this since
		 * it seems like a waste. Oh, well.
		 */
		switch (hismode) {
		case MODE_ACTIVE:
			if (process_packet(peer, pkt,
			    &(rbufp->recv_time),
				has_mac, trustable) == 0) {
				unpeer(peer);
				clock_select();
				fast_xmit(rbufp, (int)hismode, is_authentic);
			}
			break;

		case MODE_PASSIVE:
		case MODE_SERVER:
		case MODE_BROADCAST:
			/*
			 * These are errors.  Just ignore the packet.
			 * If he doesn't straighten himself out this
			 * association will eventually be disolved.
			 */
			break;

		case MODE_CLIENT:
			fast_xmit(rbufp, hismode, is_authentic);
			return;
		}
		break;

	
	case MODE_BCLIENT:
		/*
		 * Broadcast client pseudo-mode. We accept both server
		 * and broadcast data. Passive mode data is an error.
		 */
		switch (hismode) {
		case MODE_ACTIVE:
			/*
			 * This guy wants to give us real time when
			 * we've been existing on lousy broadcasts!
			 * Create a passive mode association and do it
			 * that way, but keep the old one in case the
			 * packet turns out to be bad.
			 */
			peer2 = newpeer(&rbufp->recv_srcadr,
			    rbufp->dstadr, MODE_PASSIVE,
			    PKT_VERSION(pkt->li_vn_mode),
			    NTP_MINDPOLL, NTP_MAXPOLL, 0, hiskeyid);
			if (process_packet(peer2, pkt,
			    &rbufp->recv_time, has_mac, trustable) == 0) {
				/*
				 * Strange situation.  We've been
				 * receiving broadcasts from him which
				 * we liked, but we don't like his
				 * active mode stuff. Keep his old peer
				 * structure and send him some time
				 * quickly, we'll figure it out later.
				 */
				unpeer(peer2);
				fast_xmit(rbufp, (int)hismode,
				    is_authentic);
			} else
				/*
				 * Drop the old association
				 */
				unpeer(peer);
			break;
		
		case MODE_PASSIVE:
			break;
		
		case MODE_SERVER:
		case MODE_BROADCAST:
			process_packet(peer, pkt, &rbufp->recv_time,
			    has_mac, trustable);
			/*
			 * We don't test for invalid headers.
			 * Let him time out.
			 */
			break;
		}
		break;

	case MODE_MCLIENT:
		/*
		 * This mode is temporary and does not appear outside
		 * this routine. It lasts only from the time the
		 * broadcast/multicast is recognized until the
		 * association is instantiated. Note that we start up in
		 * client/server mode to initially synchronize the
	 	 * clock.
		 */
		switch (hismode) {
		case MODE_BROADCAST:
		    	peer->flags |= FLAG_MCAST1 | FLAG_MCAST2;
			peer->hmode = MODE_CLIENT;
			process_packet(peer, pkt, &rbufp->recv_time,
			    has_mac, trustable);
			break;

		case MODE_SERVER:
		case MODE_PASSIVE:
		case MODE_ACTIVE:
		case MODE_CLIENT:
			break;
		}
	}
}


/*
 * process_packet - Packet Procedure, a la Section 3.4.3 of the
 *	specification. Or almost, at least. If we're in here we have a
 *	reasonable expectation that we will be having a long term
 *	relationship with this host.
 */
int
process_packet(peer, pkt, recv_ts, has_mac, trustable)
	register struct peer *peer;
	register struct pkt *pkt;
	l_fp *recv_ts;
	int has_mac;
	int trustable;			/* used as "valid header" */
{
	l_fp t10, t23;
	s_fp di, ei, p_dist, p_disp;
	l_fp ci, p_rec, p_xmt, p_org;
	int randomize;
	u_char ostratum, oreach;
	U_LONG temp;
	u_fp precision;

	sys_processed++;
	peer->processed++;
	p_dist = NTOHS_FP(pkt->rootdelay);
	p_disp = NTOHS_FP(pkt->rootdispersion);
	NTOHL_FP(&pkt->rec, &p_rec);
	NTOHL_FP(&pkt->xmt, &p_xmt);
	if (PKT_MODE(pkt->li_vn_mode) != MODE_BROADCAST)
		NTOHL_FP(&pkt->org, &p_org);
	else
		p_org = peer->rec;
	peer->rec = *recv_ts;
	peer->flash = 0;
	randomize = POLL_RANDOMCHANGE;

	/*
	 * Test for old or duplicate packets (tests 1 through 3).
	 */
	if (L_ISHIS(&peer->org, &p_xmt))	/* count old packets */
		peer->oldpkt++;
	if (L_ISEQU(&peer->org, &p_xmt))	/* test 1 */
		peer->flash |= TEST1;		/* duplicate packet */
	if (PKT_MODE(pkt->li_vn_mode) != MODE_BROADCAST) {
		if (!L_ISEQU(&peer->xmt, &p_org)) { /* test 2 */
			randomize = POLL_MAKERANDOM;
			peer->bogusorg++;
			peer->flash |= TEST2;	/* bogus packet */
		}
		if (L_ISZERO(&p_rec) || L_ISZERO(&p_org))
			peer->flash |= TEST3;	/* unsynchronized */
	} else {
		if (L_ISZERO(&p_org))
			peer->flash |= TEST3;   /* unsynchronized */
	}
	peer->org = p_xmt;	/* reuse byte-swapped pkt->xmt */
	peer->ppoll = pkt->ppoll;
		
	/*
	 * Call poll_update(). This will either start us, if the
	 * association is new, or drop the polling interval if the
	 * association is existing and ppoll has been reduced.
	 */
	poll_update(peer, peer->hpoll, randomize);


	/*
	 * Test for valid header (tests 5 through 8)
	 */
	if (trustable == 0)		/* test 5 */
		peer->flash |= TEST5;	/* authentication failed */
	temp = ntohl(pkt->reftime.l_ui);
	if (PKT_LEAP(pkt->li_vn_mode) == LEAP_NOTINSYNC || /* test 6 */
	    p_xmt.l_ui < temp || p_xmt.l_ui >= temp + NTP_MAXAGE)
		peer->flash |= TEST6;	/* peer clock unsynchronized */
	if (!(peer->flags & FLAG_CONFIG) &&  /* test 7 */
	    (PKT_TO_STRATUM(pkt->stratum) >= NTP_MAXSTRATUM ||
	    PKT_TO_STRATUM(pkt->stratum) > sys_stratum))
		peer->flash |= TEST7;	/* peer stratum out of bounds */
	if (p_dist >= NTP_MAXDISPERSE	/* test 8 */
	    || p_dist <= (-NTP_MAXDISPERSE)
	    || p_disp >= NTP_MAXDISPERSE)
		peer->flash |= TEST8;	/* delay/dispersion too big */

	/*
	 * If the packet header is invalid (tests 5 through 8), exit
	 */
	if (peer->flash & (TEST5 | TEST6 | TEST7 | TEST8)) {

#ifdef DEBUG
	    if (debug > 1)
		printf("invalid packet header %s %02x\n",
		       ntoa(&peer->srcadr), peer->flash);
#endif

		return(0);
	}

	/*
	 * Valid header; update our state.
	 */
	peer->leap = PKT_LEAP(pkt->li_vn_mode);
	peer->pmode = PKT_MODE(pkt->li_vn_mode);
	if (has_mac)
		peer->pkeyid = ntohl(pkt->keyid);
	else
		peer->pkeyid = 0;
	ostratum = peer->stratum;
	peer->stratum = PKT_TO_STRATUM(pkt->stratum);
	peer->precision = pkt->precision;
	peer->rootdelay = p_dist;
	peer->rootdispersion = p_disp;
	peer->refid = pkt->refid;
	NTOHL_FP(&pkt->reftime, &peer->reftime);
	oreach = peer->reach;
	if (peer->reach == 0) {
		peer->timereachable = current_time;
		/*
		 * If this guy was previously unreachable, set his
		 * polling interval to the minimum and reset the
		 * unreach counter.
		 */
		peer->unreach = 0;
		peer->hpoll = peer->minpoll;
	}
	peer->reach |= 1;

	/*
	 * If running in a client/server association, calculate the
	 * clock offset c, roundtrip delay d and dispersion e. We use
	 * the equations (reordered from those in the spec). Note that,
	 * in a broadcast association, org has been set to the time of
	 * last reception. Note the computation of dispersion includes
	 * the system precision plus that due to the frequency error
	 * since the originate time.
	 *
	 * c = ((t2 - t3) + (t1 - t0)) / 2
	 * d = (t2 - t3) - (t1 - t0)
	 * e = (org - rec) (seconds only)
	 */
	t10 = p_xmt;			/* compute t1 - t0 */
	L_SUB(&t10, &peer->rec);
	t23 = p_rec;			/* compute t2 - t3 */
	L_SUB(&t23, &p_org);
	ci = t10;
	precision = FP_SECOND >> -(int)sys_precision;
	if (precision == 0)
		precision = 1;
	ei = precision + peer->rec.l_ui - p_org.l_ui;

	/*
	 * If running in a broacast association, the clock offset is (t1
	 * - t0) corrected by the one-way delay, but we can't measure
	 * that directly; therefore, we start up in client/server mode,
	 * calculate the clock offset, using the engineered refinement
	 * algorithms, while also receiving broadcasts. When a broadcast
	 * is received in client/server mode, we calculate a correction
	 * factor to use after switching back to broadcast mode. We know
	 * NTP_SKEWFACTOR == 16, which accounts for the simplified ei
	 * calculation.
	 *
	 * If FLAG_MCAST2 is set, we are a broadcast/multicast client.
	 * If FLAG_MCAST1 is set, we haven't calculated the propagation
	 * delay. If hmode is MODE_CLIENT, we haven't set the local
	 * clock in client/server mode. Initially, we come up
	 * MODE_CLIENT. When the clock is first updated and FLAG_MCAST2
	 * is set, we switch from MODE_CLIENT to MODE_BCLIENT.
	 */
	if (peer->pmode == MODE_BROADCAST) {
		if (peer->flags & FLAG_MCAST1) {
			if (peer->hmode == MODE_BCLIENT)
				peer->flags &= ~FLAG_MCAST1;
			L_SUB(&ci, &peer->offset);
			L_NEG(&ci);
			peer->estbdelay = LFPTOFP(&ci);
			return (1);

		}
		FPTOLFP(peer->estbdelay, &t10);
		L_ADD(&ci, &t10);
		di = peer->delay;

	} else {
		L_ADD(&ci, &t23);
		L_RSHIFT(&ci);
		L_SUB(&t23, &t10);
		di = LFPTOFP(&t23);
	}

#ifdef DEBUG
	if (debug > 3)
		printf("offset: %s, delay %s, error %s\n",
		       lfptoa(&ci, 6), fptoa(di, 5), fptoa(ei, 5));
#endif
	if (di >= NTP_MAXDISPERSE || di <= (-NTP_MAXDISPERSE)
	    || ei >= NTP_MAXDISPERSE)	/* test 4 */
		peer->flash |= TEST4;	/* delay/dispersion too big */

	/*
	 * If the packet data is invalid (tests 1 through 4), exit.
	 */
	if (peer->flash) {

#ifdef DEBUG
		if (debug)
			printf("invalid packet data %s %02x\n",
			    ntoa(&peer->srcadr), peer->flash);
#endif

		/*
		 * If there was a reachability change report it even
		 * though the packet was bogus.
		 */
		if (oreach == 0)
			report_event(EVNT_REACH, peer);
		return(1);
	}

	/*
	 * This one is valid.  Mark it so, give it to clock_filter().
	 */
	clock_filter(peer, &ci, di, (u_fp)ei);

	/*
	 * If this guy was previously unreachable, report him reachable.
	 * Note we do this here so that the peer values we return are
	 * the updated ones.
	 */
	if (oreach == 0)
		report_event(EVNT_REACH, peer);

	/*
	 * Now update the clock. If we have found a system peer and this
	 * is a broadcast/multicast client, switch to listen mode.
	 */
	clock_update(peer);
	if (sys_peer && peer->flags & FLAG_MCAST2)
		peer->hmode = MODE_BCLIENT;
	return(1);
}


/*
 * clock_update - Clock-update procedure, see section 3.4.5.
 */
void
clock_update(peer)
	struct peer *peer;
{
	u_char oleap;
	u_char ostratum;
	s_fp d;
	extern u_char leap_mask;

#ifdef DEBUG
	if (debug)
		printf("clock_update(%s)\n", ntoa(&peer->srcadr));
#endif

	record_peer_stats(&peer->srcadr, ctlpeerstatus(peer),
	    &peer->offset, peer->delay, peer->dispersion);

	/*
	 * Call the clock selection algorithm to see if this update
	 * causes the peer to change. If this is not the system peer,
	 * quit now.
	 */
	clock_select();
	if (peer != sys_peer)
		return;

	/*
	 * Update the system state. This updates the system stratum,
	 * leap bits, root delay, root dispersion, reference ID and
	 * reference time. We also update select dispersion and max
	 * frequency error.
	 */
	oleap = sys_leap;
	ostratum = sys_stratum;
	sys_stratum = peer->stratum + 1;
	if (sys_stratum == 1)
        	sys_refid = peer->refid;
	else
		sys_refid = peer->srcadr.sin_addr.s_addr;
	sys_reftime = peer->rec;
	d = peer->delay;
	if (d < 0)
		d = -d;
 	sys_rootdelay = peer->rootdelay + d;
	d = peer->soffset;
	if (d < 0)
		d = -d;
	d += peer->dispersion + peer->selectdisp;
	if (!peer->flags & FLAG_REFCLOCK && d < NTP_MINDISPERSE)
		d = NTP_MINDISPERSE;
	sys_rootdispersion = peer->rootdispersion + d;

	/*
	 * Reset/adjust the system clock. Watch for timewarps here.
	 */
	switch (local_clock(&sys_offset, peer)) {
	case -1:

		/*
		 * Clock is too screwed up. Just exit for now.
		 */
		report_event(EVNT_SYSFAULT, (struct peer *)0);
		exit(1);
		/*NOTREACHED*/
	case 0:

		/*
		 * Clock was slewed.  Continue on normally.
		 */
		sys_leap = leap_consensus & leap_mask;
		L_CLR(&sys_refskew);
		break;
		
	case 1:

		/*
		 * Clock was stepped.  Clear filter registers
		 * of all peers.
		 */
		clear_all();
		leap_process();		/* reset the leap interrupt */
		sys_leap = LEAP_NOTINSYNC;
		sys_refskew.l_i = NTP_MAXSKEW; sys_refskew.l_f = 0;
		report_event(EVNT_CLOCKRESET, (struct peer *)0);
		break;
	}
	sys_maxd = peer->dispersion + peer->selectdisp;
	if (oleap != sys_leap)
		report_event(EVNT_SYNCCHG, (struct peer *)0);
	if (ostratum != sys_stratum)
		report_event(EVNT_PEERSTCHG, (struct peer *)0);
}


/*
 * poll_update - update peer poll interval. See Section 3.4.8 of the
 *     spec.
 */
void
poll_update(peer, new_hpoll, randomize)
	struct peer *peer;
	unsigned int new_hpoll;
	int randomize;
{
	register struct event *evp;
	register u_long new_timer;
	u_char newpoll, oldpoll;

#ifdef DEBUG
	if (debug > 1)
		printf("poll_update(%s, %d, %d)\n", ntoa(&peer->srcadr),
		    new_hpoll, randomize);
#endif
	/*
	 * Catch reference clocks here.  The polling interval for a
	 * reference clock is fixed and needn't be maintained by us.
	 */
	if (peer->flags & FLAG_REFCLOCK || peer->hmode ==
	    MODE_BROADCAST)
		return;

	/*
	 * This routine * will randomly perturb the new peer.timer if
	 * requested, to try to prevent synchronization with the remote
	 * peer from occuring.  There are three options, based on the
	 * value of randomize:
	 *
	 * POLL_NOTRANDOM - essentially the spec algorithm.  If
	 * peer.timer is greater than the new polling interval,
	 * drop it to the new interval.
	 *
	 * POLL_RANDOMCHANGE - make changes randomly.  If peer.timer
	 * must be changed, based on the comparison about, randomly
	 * perturb the new value of peer.timer.
	 *
	 * POLL_MAKERANDOM - make next interval random.  Calculate
	 * a randomly perturbed poll interval.  If this value is
	 * less that peer.timer, update peer.timer.
	 */
	oldpoll = peer->hpoll;
	if (peer->hmode == MODE_BCLIENT)
		peer->hpoll = peer->ppoll;
	else if ((peer->flags & FLAG_SYSPEER) && new_hpoll > sys_poll)
		peer->hpoll = max(peer->minpoll, sys_poll);
	else {
		if (new_hpoll > peer->maxpoll)
			peer->hpoll = peer->maxpoll;
		else if (new_hpoll < peer->minpoll)
			peer->hpoll = peer->minpoll;
		else
			peer->hpoll = new_hpoll;
	}

	/* hpoll <= maxpoll for sure */
	newpoll = max((u_char)min(peer->ppoll, peer->hpoll),
	    peer->minpoll);
	if (randomize == POLL_MAKERANDOM || (randomize ==
	     POLL_RANDOMCHANGE && newpoll != oldpoll))
		new_timer = (1 << (newpoll - 1))
		    + ranp2(newpoll - 1) + current_time;
	else
		new_timer = (1 << newpoll) + current_time;
	evp = &(peer->event_timer);
	if (evp->next == 0 || evp->event_time > new_timer) {
		TIMER_DEQUEUE(evp);
		evp->event_time = new_timer;
		TIMER_ENQUEUE(timerqueue, evp);
	}
}

/*
 * clear_all - clear all peer filter registers.  This is done after
 *	       a step change in the time.
 */
static void
clear_all()
{
	register int i;
	register struct peer *peer;

	for (i = 0; i < HASH_SIZE; i++)
		for (peer = peer_hash[i]; peer != 0; peer = peer->next) {
			peer_clear(peer);
		}

	/*
	 * Clear sys_peer. We'll sync to one later.
	 */
	sys_peer = 0;
	sys_stratum = STRATUM_UNSPEC;
}


/*
 * clear - clear peer filter registers.  See Section 3.4.7 of the spec.
 */
void
peer_clear(peer)
	register struct peer *peer;
{
	register int i;

#ifdef DEBUG
	if (debug)
		printf("clear(%s)\n", ntoa(&peer->srcadr));
#endif
	memset(CLEAR_TO_ZERO(peer), 0, LEN_CLEAR_TO_ZERO);
	peer->hpoll = peer->minpoll;
	peer->dispersion = NTP_MAXDISPERSE;
	for (i = 0; i < NTP_SHIFT; i++)
		peer->filter_error[i] = NTP_MAXDISPERSE;
	poll_update(peer, peer->minpoll, POLL_RANDOMCHANGE);
	clock_select();

	/*
	 * Clear out the selection counters
	 */
	peer->candidate = 0;
	peer->select = 0;
	peer->correct = 0;
	peer->was_sane = 0;

	/*
	 * Since we have a chance to correct possible funniness in
	 * our selection of interfaces on a multihomed host, do so
	 * by setting us to no particular interface.
	 */
	peer->dstadr = any_interface;
}


/*
 * clock_filter - add incoming clock sample to filter register and run
 *		  the filter procedure to find the best sample.
 */
void
clock_filter(peer, sample_offset, sample_delay, sample_error)
	register struct peer *peer;
	l_fp *sample_offset;
	s_fp sample_delay;
	u_fp sample_error;
{
	register int i, j, k, n;
	register u_char *ord;
	s_fp distance[NTP_SHIFT];
	long skew, skewmax;

#ifdef DEBUG
	if (debug)
		printf("clock_filter(%s, %s, %s, %s)\n",
		    ntoa(&peer->srcadr), lfptoa(sample_offset, 6),
		    fptoa(sample_delay, 5), ufptoa(sample_error, 5));
#endif

	/*
	 * Update sample errors and calculate distances. Also initialize
	 * sort index vector. We know NTP_SKEWFACTOR == 16
	 */
	skew = sys_clock - peer->update;
	peer->update = sys_clock;
	ord = peer->filter_order;
	j = peer->filter_nextpt;
	for (i = 0; i < NTP_SHIFT; i++) {
		peer->filter_error[j] += (u_fp)skew;
		if (peer->filter_error[j] > NTP_MAXDISPERSE)
			 peer->filter_error[j] = NTP_MAXDISPERSE;
		distance[i] = peer->filter_error[j] +
		    (peer->filter_delay[j] >> 1);
		ord[i] = j;
		if (--j < 0)
			j += NTP_SHIFT;
	}

	/*
	 * Insert the new sample at the beginning of the register.
	 */
	peer->filter_delay[peer->filter_nextpt] = sample_delay;
	peer->filter_offset[peer->filter_nextpt] = *sample_offset;
	peer->filter_soffset[peer->filter_nextpt] =
	    LFPTOFP(sample_offset);
	peer->filter_error[peer->filter_nextpt] = sample_error;
	distance[0] = sample_error + (sample_delay >> 1);

	/*
	 * Sort the samples in the register by distance. The winning
	 * sample will be in ord[0]. Sort the samples only if the
	 * samples are not too old and the delay is meaningful.
	 */
	skewmax = 0;
	for (n = 0; n < NTP_SHIFT && sample_delay; n++) {
		for (j = 0; j < n && skewmax <
		    CLOCK_MAXSEC; j++) {
			if (distance[j] > distance[n]) {
				s_fp ftmp;

				ftmp = distance[n];
				k = ord[n];
				distance[n] = distance[j];
				ord[n] = ord[j];
				distance[j] = ftmp;
				ord[j] = k;
			}
		}
		skewmax += (1 << peer->hpoll);
	} 
	peer->filter_nextpt++;
	if (peer->filter_nextpt >= NTP_SHIFT)
		peer->filter_nextpt = 0;
	
	/*
	 * We compute the dispersion as per the spec. Note that, to make
	 * things simple, both the l_fp and s_fp offsets are retained
	 * and that the s_fp could be nonsense if the l_fp is greater
	 * than about 32000 s. However, the sanity checks in
	 * ntp_loopfilter() require the l_fp offset to be less than 1000
	 * s anyway, so not to worry.
	 */
	if (peer->filter_error[ord[0]] >= NTP_MAXDISPERSE) {
		peer->dispersion = NTP_MAXDISPERSE;
	} else {
		s_fp d;
		u_fp y;

		peer->delay = peer->filter_delay[ord[0]];
		peer->offset = peer->filter_offset[ord[0]];
		peer->soffset = LFPTOFP(&peer->offset);
		peer->dispersion = peer->filter_error[ord[0]];

		y = 0;
		for (i = NTP_SHIFT - 1; i > 0; i--) {
			if (peer->filter_error[ord[i]] >=
			    NTP_MAXDISPERSE)
				d = NTP_MAXDISPERSE;
			else {
				d = peer->filter_soffset[ord[i]] -
				    peer->filter_soffset[ord[0]];
				if (d < 0)
					d = -d;
				if (d > NTP_MAXDISPERSE)
					d = NTP_MAXDISPERSE;
			}
			/*
			 * XXX This *knows* NTP_FILTER is 1/2
			 */
			y = ((u_fp)d + y) >> 1;
		}
		peer->dispersion += y;

		/*
		 * Calculate synchronization distance backdated to
		 * sys_lastselect (clock_select will fix it). We know
		 * NTP_SKEWFACTOR == 16.
		 */
		d = peer->delay;
		if (d < 0)
			d = -d;
		d += peer->rootdelay;
		peer->synch = (d >> 1) + peer->rootdispersion +
		    peer->dispersion - (sys_clock - sys_lastselect);
	}
}


/*
 * clock_select - find the pick-of-the-litter clock
 */
void
clock_select()
{
	register struct peer *peer;
	register int i;
	register int nlist, nl3;
	register s_fp d, e;
	register int j;
	register int n;
	register int allow, found, k;
	s_fp low = 0x7fffffff;
	s_fp high = -0x7ffffff;
	u_fp synch[NTP_MAXCLOCK], error[NTP_MAXCLOCK];
	struct peer *osys_peer;
	struct peer *typeacts = 0;
	struct peer *typelocal = 0;
	struct peer *typepps = 0;
	struct peer *typeprefer = 0;
	struct peer *typesystem = 0;

	static int list_alloc = 0;
	static struct endpoint *endpoint;
	static int *index;
	static struct peer **peer_list;
	static int endpoint_size = 0, index_size = 0, peer_list_size = 0;

#ifdef DEBUG
	if (debug > 1)
		printf("clock_select()\n");
#endif

	/*
	 * Initizialize. If a prefer peer does not survive this thing,
	 * the pps_update switch will remain zero.
	 */
	pps_update = 0;
	nlist = 0;
	for (n = 0; n < HASH_SIZE; n++)
		nlist += peer_hash_count[n];
	if (nlist > list_alloc) {
		if (list_alloc > 0) {
			free(endpoint);
			free(index);
			free(peer_list);
		}
		while (list_alloc < nlist) {
			list_alloc += 5;
			endpoint_size += 5 * 3 * sizeof *endpoint;
			index_size += 5 * 3 * sizeof *index;
			peer_list_size += 5 * sizeof *peer_list;
		}
		endpoint = (struct endpoint *)emalloc(endpoint_size);
		index = (int *)emalloc(index_size);
		peer_list = (struct peer **)emalloc(peer_list_size);
	}

	/*
	 * This first chunk of code is supposed to go through all
	 * peers we know about to find the NTP_MAXLIST peers which
	 * are most likely to succeed.  We run through the list
	 * doing the sanity checks and trying to insert anyone who
	 * looks okay.  We are at all times aware that we should
	 * only keep samples from the top two strata and we only need
	 * NTP_MAXLIST of them.
	 */
	nlist = nl3 = 0;	/* none yet */
	for (n = 0; n < HASH_SIZE; n++) {
		for (peer = peer_hash[n]; peer != 0; peer = peer->next) {
			/*
			 * Clear peer selection stats
			 */
			peer->was_sane = 0;
			peer->correct = 0;
			peer->candidate = 0;
			peer->select = 0;

			peer->flags &= ~FLAG_SYSPEER;
			/*
			 * Update synch distance (NTP_SKEWFACTOR == 16).
			 * Note synch distance check instead of spec
			 * dispersion check. Naughty.
			 */
			peer->synch += (sys_clock - sys_lastselect);

			if (peer->reach == 0)
				continue;	/* unreachable */
			if (peer->stratum > 1 && peer->refid ==
			    peer->dstadr->sin.sin_addr.s_addr)
				continue;	/* sync loop */
			if (peer->stratum >= NTP_MAXSTRATUM ||
	    		    peer->stratum > sys_stratum)
				continue;	/* bad stratum  */

			if (peer->dispersion >= NTP_MAXDISTANCE) {
				peer->seldisptoolarge++;
				continue; /* too noisy or broken */
			}
			if (peer->org.l_ui < peer->reftime.l_ui) {
				peer->selbroken++;
				continue;	/* very broken host */
			}

			/*
			 * Don't allow the local-clock or acts drivers
			 * in the kitchen at this point, unless the
			 * prefer peer. Do that later, but only if
			 * nobody else is around.
			 */
			if (peer->refclktype == REFCLK_LOCALCLOCK) {
				typelocal = peer;
				if (!(peer->flags & FLAG_PREFER))
					continue; /* no local clock */
			}
			if (peer->refclktype == REFCLK_NIST_ACTS) {
				typeacts = peer;
				if (!(peer->flags & FLAG_PREFER))
					continue; /* no acts */
			}

			/*
			 * If we get this far, we assume the peer is
			 * acceptable.
			 */
			peer->was_sane = 1;
			peer_list[nlist++] = peer;

			/*
			 * Insert each interval endpoint on the sorted
			 * list.
			 */
			e = peer->soffset + peer->synch; /* Upper end */
			for (i = nl3 - 1; i >= 0; i--) {
				if (e >= endpoint[index[i]].val)
					break;
				index[i + 3] = index[i];
			}
			index[i + 3] = nl3;
			endpoint[nl3].type = 1;
			endpoint[nl3++].val = e;

			e -= peer->synch;	/* Center point */
			for ( ; i >= 0; i--) {
				if (e >= endpoint[index[i]].val)
					break;
				index[i + 2] = index[i];
			}
			index[i + 2] = nl3;
			endpoint[nl3].type = 0;
			endpoint[nl3++].val = e;

			e -= peer->synch;	/* Lower end */
			for ( ; i >= 0; i--) {
				if (e >= endpoint[index[i]].val)
					break;
				index[i + 1] = index[i];
			}
			index[i + 1] = nl3;
			endpoint[nl3].type = -1;
			endpoint[nl3++].val = e;
		}
	}
	sys_lastselect = sys_clock;

#ifdef DEBUG
	if (debug > 2)
		for (i = 0; i < nl3; i++)
			printf("select: endpoint %2d %s\n",
			       endpoint[index[i]].type,
			       fptoa(endpoint[index[i]].val, 6));
#endif

	i = 0;
	j = nl3 - 1;
	allow = nlist;		/* falsetickers assumed */
	found = 0;
	while (allow > 0) {
		allow--;
		for (n = 0; i <= j; i++) {
			n += endpoint[index[i]].type;
			if (n < 0)
				break;
			if (endpoint[index[i]].type == 0)
				found++;
		}
		for (n = 0; i <= j; j--) {
			n += endpoint[index[j]].type;
			if (n > 0)
				break;
			if (endpoint[index[j]].type == 0)
				found++;
		}
		if (found > allow)
			break;
		low = endpoint[index[i++]].val;
		high = endpoint[index[j--]].val;
	}

	/*
	 * If no survivors remain at this point, check if the acts or
	 * local clock drivers have been found. If so, nominate one of
	 * them as the only survivor. Otherwise, give up and declare us
	 * unsynchronized.
	 */
	if ((allow << 1) >= nlist) {
		if (typeacts != 0) {
			typeacts->was_sane = 1;
			peer_list[0] = typeacts;
			nlist = 1;
		} else if (typelocal != 0) {
			typelocal->was_sane = 1;
			peer_list[0] = typelocal;
			nlist = 1;
		} else {
			if (sys_peer != 0)
				report_event(EVNT_PEERSTCHG,
				    (struct peer *)0);
			sys_peer = 0;
			sys_stratum = STRATUM_UNSPEC;
			return;
		}
	}

#ifdef DEBUG
	if (debug > 2)
		printf("select: low %s high %s\n", fptoa(low, 6),
		       fptoa(high, 6));
#endif

	/*
	 * Clustering algorithm. Process intersection list to discard
	 * outlyers. Construct candidate list in cluster order
	 * determined by the sum of peer synchronization distance plus
	 * scaled stratum. We must find at least one peer.
	 */
	j = 0;
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		if (nlist > 1 && (peer->soffset < low || high <
		    peer->soffset))
			continue;
		peer->correct = 1;
		d = peer->synch + ((u_long)peer->stratum <<
		    NTP_DISPFACTOR);
		if (j >= NTP_MAXCLOCK) {
			if (d >= synch[j - 1])
				continue;
			else
				j--;
		}
		for (k = j; k > 0; k--) {
			if (d >= synch[k - 1])
				break;
			synch[k] = synch[k - 1];
			peer_list[k] = peer_list[k - 1];
		}
		peer_list[k] = peer;
		synch[k] = d;
		j++;
	}
	nlist = j;

#ifdef DEBUG
	if (debug > 2)
		for (i = 0; i < nlist; i++)
			printf("select: candidate %s cdist %s\n",
			       ntoa(&peer_list[i]->srcadr),
			       fptoa(synch[i], 6));
#endif

	/*
	 * Now, prune outlyers by root dispersion. Continue as long as
	 * there are more than NTP_MINCLOCK survivors and the minimum
	 * select dispersion is greater than the maximum peer
	 * dispersion. Stop if we are about to discard a preferred peer.
	 */
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		peer->candidate = i + 1;
		error[i] = peer_list[i]->rootdispersion +
			   peer_list[i]->dispersion +
			   (sys_clock - peer_list[i]->update);
	}
	while (1) {
		u_fp maxd = 0;
		e = error[0];
		for (k = i = nlist - 1; i >= 0; i--) {
			u_fp sdisp = 0;

			for (j = nlist - 1; j > 0; j--) {
				d = peer_list[i]->soffset
				    - peer_list[j]->soffset;
				if (d < 0)
					d = -d;
				sdisp += d;
				sdisp = ((sdisp >> 1) + sdisp) >> 1;
			}
			peer_list[i]->selectdisp = sdisp;
			if (sdisp > maxd) {
				maxd = sdisp;
				k = i;
			}
			if (error[i] < e)
				e = error[i];
		}
		if (nlist <= NTP_MINCLOCK || maxd <= e ||
		    peer_list[k]->flags & FLAG_PREFER)
			break;
		for (j = k + 1; j < nlist; j++) {
			peer_list[j - 1] = peer_list[j];
			error[j - 1] = error[j];
		}
		nlist--;
	}
	
#ifdef DEBUG
	if (debug > 1) {
		for (i = 0; i < nlist; i++)
			printf("select: survivor %s offset %s, cdist %s\n",
			       ntoa(&peer_list[i]->srcadr),
			       lfptoa(&peer_list[i]->offset, 6),
			       fptoa(synch[i], 5));
	}
#endif

	/*
	 * What remains is a list of not greater than NTP_MINCLOCK
	 * peers. We want only a peer at the lowest stratum to become
	 * the system peer, although all survivors are eligible for the
	 * combining algorithm. First record their order, diddle the
	 * flags and clamp the poll intervals. Then, consider the peers
	 * at the lowest stratum. Of these, OR the leap bits on the
	 * assumption that, if some of them honk nonzero bits, they must
	 * know what they are doing. Also, check for prefer and pps
	 * peers. If a prefer peer is found within CLOCK_MAX, update the
	 * pps switch. Of the other peers not at the lowest stratum,
	 * check if the system peer is among them and, if found, zap
	 * him. We note that the head of the list is at the lowest
	 * stratum and that unsynchronized peers cannot survive this
	 * far.
	 */
	leap_consensus = 0;
	for (i = nlist - 1; i >= 0; i--) {
		peer_list[i]->select = i + 1;
		peer_list[i]->flags |= FLAG_SYSPEER;
		poll_update(peer_list[i], peer_list[i]->hpoll,
		    POLL_RANDOMCHANGE);
		if (peer_list[i]->stratum == peer_list[0]->stratum) {
			leap_consensus |= peer_list[i]->leap;
			if (peer_list[i]->refclktype == REFCLK_ATOM_PPS)
				typepps = peer_list[i];
			if (peer_list[i] == sys_peer)
				typesystem = peer_list[i];
			if (peer_list[i]->flags & FLAG_PREFER) {
				typeprefer = peer_list[i];
				if (typeprefer->soffset >= -CLOCK_MAX_FP &&
				     typeprefer->soffset < CLOCK_MAX_FP)
					pps_update = 1;
			}
		} else {
			if (peer_list[i] == sys_peer)
				sys_peer = 0;
		}
	}

	/*
	 * Mitigation rules of the game. There are several types of
	 * peers that make a difference here: (1) prefer local peers
	 * (type REFCLK_LOCALCLOCK with FLAG_PREFER) or prefer acts
	 * peers (type REFCLK_NIST_ATOM with FLAG_PREFER), (2) pps peers
	 * (type REFCLK_ATOM_PPS), (3) remaining prefer peers (flag
	 * FLAG_PREFER), (4) the existing system peer, if any, (5) the
	 * head of the survivor list. Note that only one peer can be
	 * declared prefer. The order of preference is in the order
	 * stated. Note that all of these must be at the lowest stratum,
	 * i.e., the stratum of the head of the survivor list.
	 */
	osys_peer = sys_peer;
	if (typeprefer && (typeprefer == typelocal || typeprefer ==
	    typeacts || !typepps)) {
		sys_peer = typeprefer;
		sys_peer->selectdisp = 0;
		sys_offset = sys_peer->offset;
#ifdef DEBUG
		if (debug)
			printf("select: prefer offset %s\n",
			    lfptoa(&sys_offset, 6));
#endif
	} else if (typepps) {
		sys_peer = typepps;
		sys_peer->selectdisp = 0;
		sys_offset = sys_peer->offset;
#ifdef DEBUG
		if (debug)
			printf("select: pps offset %s\n",
			    lfptoa(&sys_offset, 6));
#endif
	} else {
		if (!typesystem)
			sys_peer = peer_list[0];
		clock_combine(peer_list, nlist);
#ifdef DEBUG
		if (debug)
			printf("select: combine offset %s\n",
			    lfptoa(&sys_offset, 6));
#endif
	}

	/*
	 * If we got a new system peer from all of this, report the
	 * event and clamp the system poll interval.
	 */
	if (osys_peer != sys_peer) {
		sys_poll = sys_peer->minpoll;
		report_event(EVNT_PEERSTCHG, (struct peer *)0);
	}
}

/*
 * clock_combine - combine offsets from selected peers
 *
 * Note: this routine uses only those peers at the lowest stratum.
 * Strictly speaking, this is at variance with the spec.
 */  
void
clock_combine(peers, npeers)
	struct peer **peers;
	int npeers;
{
	register int i, j, k;
	register u_fp a, b, d;
	u_fp synch[NTP_MAXCLOCK];
	l_fp coffset[NTP_MAXCLOCK];
	l_fp diff;

	/*
	 * Sort the offsets by synch distance.
	 */
	k = 0;
	for (i = 0; i < npeers; i++) {
		if (peers[i]->stratum > sys_peer->stratum)
			continue;
		d = peers[i]->synch;
		for (j = k; j > 0; j--) {
			if (synch[j - 1] <= d)
				break;
			synch[j] = synch[j - 1];
			coffset[j] = coffset[j - 1];
		}
		synch[j] = d;
		coffset[j] = peers[i]->offset;
		k++;
	}

	/*
	 * Succesively combine the two offsets with the highest
	 * distance and enter the result into the sorted list.
	 */
	for (i = k - 2; i >= 0; i--) {
		/*
		 * The possible weights for the most distant offset
		 * are 1/2, 1/4, 1/8 and zero. We combine the synch
		 * distances as if they were variances of the offsets;
		 * the given weights allow us to stay within 16/15 of
		 * the optimum combined variance at each step, and
		 * within 8/7 on any series.
		 *
		 * The breakeven points for the weigths are found
		 * where the smaller distance is 3/8, 3/16 and 1/16
		 * of the sum, respectively.
		 */
		d = synch[i];
		a = (d + synch[i + 1]) >> 2;	/* (d1+d2)/4 */
		b = a>>1;			/* (d1+d2)/8 */
		if (d <= (b>>1))		/* d1 <= (d1+d2)/16 */
			/*
			 * Below 1/16, no combination is done,
			 * we just drop the distant offset.
			 */
			continue;

		/*
		 * The offsets are combined by shifting their
		 * difference the appropriate number of times and
		 * adding it back in.
		 */
		diff = coffset[i + 1];
		L_SUB(&diff, &coffset[i]);
		L_RSHIFT(&diff);
		if (d >= a + b) {		/* d1 >= 3(d1+d2)/8 */
			/*
			 * Above 3/8, the weight is 1/2, and the
			 * combined distance is (d1+d2)/4
			 */
			d = a;
		} else {
			a >>= 2;		/* (d1+d2)/16 */
			L_RSHIFT(&diff);
			if (d >= a + b) {	/* d1 >= 3(d1+d2)/16 */
				/*
				 * Between 3/16 and 3/8, the weight
				 * is 1/4, and the combined distance
				 * is (9d1+d2)/16 = d1/2 + (d1+d2)/16
				 */
				d = (d>>1) + a;
			} else {
				/*
				 * Between 1/16 and 3/16, the weight
				 * is 1/8, and the combined distance
				 * is (49d1+d2)/64 = 3d1/4+(d1+d2)/64
				 * (We know d > a, so the shift is safe).
				 */
				L_RSHIFT(&diff);
				d -= (d - a)>>2;
			}
		}
		/*
		 * Now we can make the combined offset and insert it
		 * in the list.
		 */
		L_ADD(&diff, &coffset[i]);
		for (j = i; j > 0; j--) {
			if (d >= synch[j - 1])
				break;
			synch[j] = synch[j - 1];
			coffset[j] = coffset[j - 1];
		}
		synch[j] = d;
		coffset[j] = diff;
	}

	/*
	 * The result is put where clock_update() can find it.
	 */
	sys_offset = coffset[0];
}


/*
 * fast_xmit - fast path send for stateless (non-)associations
 */
void
fast_xmit(rbufp, rmode, authentic)
	struct recvbuf *rbufp;
	int rmode;
	int authentic;
{
	struct pkt xpkt;
	register struct pkt *rpkt;
	u_char xmode;
	u_short xkey = 0;
	int docrypt = 0;
	l_fp xmt_ts;
	u_fp precision;

#ifdef DEBUG
	if (debug > 1)
		printf("fast_xmit(%s, %d)\n", ntoa(&rbufp->recv_srcadr), rmode);
#endif

	/*
	 * Make up new packet and send it quick
	 */
	rpkt = &rbufp->recv_pkt;
	if (rmode == MODE_ACTIVE)
		xmode = MODE_PASSIVE;
	else
		xmode = MODE_SERVER;

	if (rbufp->recv_length >= LEN_PKT_MAC) {
		docrypt = rbufp->recv_length - LEN_PKT_NOMAC;
		if (authentic)
			xkey = ntohl(rpkt->keyid);
	}

	xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
	    PKT_VERSION(rpkt->li_vn_mode), xmode);
	xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
	xpkt.ppoll = max(NTP_MINPOLL, rpkt->ppoll);
	xpkt.precision = sys_precision;
	xpkt.rootdelay = HTONS_FP(sys_rootdelay);
	precision = FP_SECOND >> -(int)sys_precision;
	if (precision == 0)
		precision = 1;
	xpkt.rootdispersion = HTONS_FP(sys_rootdispersion +
	    precision + LFPTOFP(&sys_refskew));
	xpkt.refid = sys_refid;
	HTONL_FP(&sys_reftime, &xpkt.reftime);
	xpkt.org = rpkt->xmt;
	HTONL_FP(&rbufp->recv_time, &xpkt.rec);

	/*
	 * If we are encrypting, do it.  Else don't.  Easy.
	 */
	if (docrypt) {
		int maclen;

		xpkt.keyid = htonl(xkey);
		auth1crypt(xkey, (U_LONG *)&xpkt, LEN_PKT_NOMAC);
		get_systime(&xmt_ts);
		L_ADDUF(&xmt_ts, sys_authdelay);
		HTONL_FP(&xmt_ts, &xpkt.xmt);
		maclen = auth2crypt(xkey, (U_LONG *)&xpkt, LEN_PKT_NOMAC);
		sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, -9, &xpkt,
			LEN_PKT_NOMAC + maclen);
	} else {
		/*
		 * Get xmt timestamp, then send it without mac field
		 */
		get_systime(&xmt_ts);
		HTONL_FP(&xmt_ts, &xpkt.xmt);
		sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, -10, &xpkt,
		    LEN_PKT_NOMAC);
	}
}

/*
 * Find the precision of this particular machine
 */
#define	DUSECS	1000000	/* us in a s */
#define	HUSECS	(1 << 20) /* approx DUSECS for shifting etc */
#define	MINSTEP	5	/* minimum clock increment (ys) */
#define MAXSTEP 20000	/* maximum clock increment (us) */
#define MINLOOPS 5	/* minimum number of step samples */

/*
 * This routine calculates the differences between successive calls to
 * gettimeofday(). If a difference is less than zero, the us field
 * has rolled over to the next second, so we add a second in us. If
 * the difference is greater than zero and less than MINSTEP, the
 * clock has been advanced by a small amount to avoid standing still.
 * If the clock has advanced by a greater amount, then a timer interrupt
 * has occurred and this amount represents the precision of the clock.
 * In order to guard against spurious values, which could occur if we
 * happen to hit a fat interrupt, we do this for MINLOOPS times and
 * keep the minimum value obtained.
 */  
int default_get_precision()
{
	struct timeval tp;
	struct timezone tzp;
	long last;
	int i;
	long diff;
	long val;
	long usec;

	usec = 0;
	val = MAXSTEP;
	GETTIMEOFDAY(&tp, &tzp);
	last = tp.tv_usec;
	for (i = 0; i < MINLOOPS && usec < HUSECS;) {
		GETTIMEOFDAY(&tp, &tzp);
		diff = tp.tv_usec - last;
		last = tp.tv_usec;
		if (diff < 0)
			diff += DUSECS;
		usec += diff;
		if (diff > MINSTEP) {
			i++;
			if (diff < val)
				val = diff;
		}
	}
	syslog(LOG_INFO, "precision = %d usec", val);
	if (usec >= HUSECS)
		val = MINSTEP;	/* val <= MINSTEP; fast machine */
	diff = HUSECS;
	for (i = 0; diff > val; i--)
		diff >>= 1;
	return (i);
}

/*
 * init_proto - initialize the protocol module's data
 */
void
init_proto()
{
	l_fp dummy;

	/*
	 * Fill in the sys_* stuff.  Default is don't listen to
	 * broadcasting, don't authenticate.
	 */
	sys_leap = LEAP_NOTINSYNC;
	sys_stratum = STRATUM_UNSPEC;
	sys_precision = (s_char)default_get_precision();
	sys_rootdelay = 0;
	sys_rootdispersion = 0;
	sys_refid = 0;
	L_CLR(&sys_reftime);
	sys_refskew.l_i = NTP_MAXSKEW; sys_refskew.l_f = 0;
	sys_peer = 0;
	sys_poll = NTP_MINPOLL;
	get_systime(&dummy);
	sys_lastselect = sys_clock;

	sys_bclient = 0;
	sys_bdelay = DEFBROADDELAY;
	sys_authenticate = 0;
	sys_authdelay = DEFAUTHDELAY;

	sys_stattime = 0;
	sys_badstratum = 0;
	sys_oldversionpkt = 0;
	sys_newversionpkt = 0;
	sys_badlength = 0;
	sys_unknownversion = 0;
	sys_processed = 0;
	sys_badauth = 0;

	/*
	 * Default these to enable
	 */
	pll_enable = 1;
	stats_control = 1;
}


/*
 * proto_config - configure the protocol module
 */
void
proto_config(item, value)
	int item;
	u_long value;
{
	/*
	 * Figure out what he wants to change, then do it
	 */
	switch (item) {
	case PROTO_PLL:
		/*
		 * Turn on/off pll clock correction
		 */
		pll_enable = (int)value;
		break;

	case PROTO_MONITOR:
		/*
		 * Turn on/off monitoring
		 */
		if (value)
			mon_start(MON_ON);
		else
			mon_stop(MON_ON);
		break;

	case PROTO_FILEGEN:
		/*
		 * Turn on/off statistics
		 */
		stats_control = (int)value;
		break;

	case PROTO_BROADCLIENT:
		/*
		 * Turn on/off facility to listen to broadcasts
		 */
		sys_bclient = (int)value;
		if (value)
			io_setbclient();
		else
			io_unsetbclient();
		break;

	case PROTO_MULTICAST_ADD:
		 /*
		  * Add muliticast group address
		  */
		sys_bclient = 1;
		io_multicast_add(value);
		break;

	case PROTO_MULTICAST_DEL:
		/*
		 * Delete multicast group address
		 */
		sys_bclient = 1;
		io_multicast_del(value);
		break;
	
	case PROTO_PRECISION:
		/*
		 * Set system precision
		 */
		sys_precision = (s_char)value;
		break;
	
	case PROTO_BROADDELAY:
		/*
		 * Set default broadcast delay (s_fp)
		 */
		if (sys_bdelay < 0)
			sys_bdelay = -(-value >> 16);
		else
			sys_bdelay = value >> 16;
		break;
	
	case PROTO_AUTHENTICATE:
		/*
		 * Specify the use of authenticated data
		 */
		sys_authenticate = (int)value;
		break;


	case PROTO_AUTHDELAY:
		/*
		 * Set authentication delay (l_fp fraction)
		 */
		sys_authdelay = value;
		break;

	default:
		/*
		 * Log this error
		 */
		syslog(LOG_ERR, "proto_config: illegal item %d, value %ld",
		    item, value);
		break;
	}
}


/*
 * proto_clr_stats - clear protocol stat counters
 */
void
proto_clr_stats()
{
	sys_badstratum = 0;
	sys_oldversionpkt = 0;
	sys_newversionpkt = 0;
	sys_unknownversion = 0;
	sys_badlength = 0;
	sys_processed = 0;
	sys_badauth = 0;
	sys_stattime = current_time;
	sys_limitrejected = 0;
}
