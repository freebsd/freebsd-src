/* ntp_proto.c,v 3.1 1993/07/06 01:11:23 jbj Exp
 * ntp_proto.c - NTP version 3 protocol machinery
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"

/*
 * System variables are declared here.  See Section 3.2 of
 * the specification.
 */
u_char	sys_leap;		/* system leap indicator */
u_char	sys_stratum;		/* stratum of system */
s_char	sys_precision;		/* local clock precision */
s_fp	sys_rootdelay;		/* distance to current sync source */
u_fp	sys_rootdispersion;	/* dispersion of system clock */
U_LONG	sys_refid;		/* reference source for local clock */
l_fp	sys_offset;		/* combined offset from clock_select */
u_fp	sys_maxd;		/* dispersion of selected peer */
l_fp	sys_reftime;		/* time we were last updated */
l_fp	sys_refskew;		/* accumulated skew since last update */
struct peer *sys_peer;		/* our current peer */
u_char	sys_poll;		/* log2 of desired system poll interval */
extern LONG	sys_clock;	/* second part of current time - now in systime.c */
LONG	sys_lastselect;		/* sys_clock at last synch-dist update */

/*
 * Non-specified system state variables.
 */
int	sys_bclient;		/* we set our time to broadcasts */
U_LONG	sys_bdelay;		/* default delay to use for broadcasting */
int	sys_authenticate;	/* authenticate time used for syncing */

U_LONG	sys_authdelay;		/* ts fraction, time it takes for encrypt() */

/*
 * Statistics counters
 */
U_LONG	sys_stattime;		/* time when we started recording */
U_LONG	sys_badstratum;		/* packets with invalid incoming stratum */
U_LONG	sys_oldversionpkt;	/* old version packets received */
U_LONG	sys_newversionpkt;	/* new version packets received */
U_LONG	sys_unknownversion;	/* don't know version packets */
U_LONG	sys_badlength;		/* packets with bad length */
U_LONG	sys_processed;		/* packets processed */
U_LONG	sys_badauth;		/* packets dropped because of authorization */
U_LONG	sys_limitrejected;	/* pkts rejected due toclient count per net */

/*
 * Imported from ntp_timer.c
 */
extern U_LONG current_time;
extern struct event timerqueue[];

/*
 * Imported from ntp_io.c
 */
extern struct interface *any_interface;

/*
 * Imported from ntp_loopfilter.c
 */
extern int pps_control;
extern U_LONG pps_update;

/*
 * The peer hash table.  Imported from ntp_peer.c
 */
extern struct peer *peer_hash[];
extern int peer_hash_count[];

/*
 * debug flag
 */
extern int debug;

static	void	clear_all	P((void));

/*
 * transmit - Transmit Procedure.  See Section 3.4.1 of the specification.
 */
void
transmit(peer)
	register struct peer *peer;
{
	struct pkt xpkt;	/* packet to send */
	U_LONG peer_timer;

	if ((peer->hmode != MODE_BROADCAST && peer->hmode != MODE_BCLIENT) ||
	    (peer->hmode == MODE_BROADCAST && sys_leap != LEAP_NOTINSYNC)) {
		U_LONG xkeyid;

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
		xpkt.li_vn_mode
		    = PKT_LI_VN_MODE(sys_leap, peer->version, peer->hmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
		xpkt.ppoll = peer->hpoll;
		xpkt.precision = sys_precision;
		xpkt.rootdelay = HTONS_FP(sys_rootdelay);
		xpkt.rootdispersion =
			HTONS_FP(sys_rootdispersion +
				 (FP_SECOND >> (-(int)sys_precision)) +
				 LFPTOFP(&sys_refskew));
		xpkt.refid = sys_refid;
		HTONL_FP(&sys_reftime, &xpkt.reftime);
		HTONL_FP(&peer->org, &xpkt.org);
		HTONL_FP(&peer->rec, &xpkt.rec);

		/*
		 * Decide whether to authenticate or not.  If so, call encrypt()
		 * to fill in the rest of the frame.  If not, just add in the
		 * xmt timestamp and send it quick.
		 */
		if (peer->flags & FLAG_AUTHENABLE) {
			int sendlen;

			xpkt.keyid = htonl(xkeyid);
			auth1crypt(xkeyid, (U_LONG *)&xpkt, LEN_PKT_NOMAC);
			get_systime(&peer->xmt);
			L_ADDUF(&peer->xmt, sys_authdelay);
			HTONL_FP(&peer->xmt, &xpkt.xmt);
			sendlen = auth2crypt(xkeyid, (U_LONG *)&xpkt,
					     LEN_PKT_NOMAC);
			sendpkt(&(peer->srcadr), peer->dstadr, &xpkt,
				sendlen + LEN_PKT_NOMAC);
#ifdef DEBUG
			if (debug > 1)
				printf("transmit auth to %s\n",
				    ntoa(&(peer->srcadr)));
#endif
			peer->sent++;
		} else {
			/*
			 * Get xmt timestamp, then send it without mac field
			 */
			get_systime(&(peer->xmt));
			HTONL_FP(&peer->xmt, &xpkt.xmt);
			sendpkt(&(peer->srcadr), peer->dstadr, &xpkt,
			    LEN_PKT_NOMAC);
#ifdef DEBUG
			if (debug > 1)
				printf("transmit to %s\n", ntoa(&(peer->srcadr)));
#endif
			peer->sent++;
		}
	}

	if (peer->hmode != MODE_BROADCAST) {
		u_char opeer_reach;
		/*
		 * Determine reachability and diddle things if we
		 * haven't heard from the host for a while.
		 */
		opeer_reach = peer->reach;
		peer->reach <<= 1;
		if (peer->reach == 0) {
			if (opeer_reach != 0)
				report_event(EVNT_UNREACH, peer);
			/*
			 * Clear this guy out.  No need to redo clock
			 * selection since by now this guy won't be a player
			 */
			if (peer->flags & FLAG_CONFIG) {
				if (opeer_reach != 0) {
					peer_clear(peer);
					peer->timereachable = current_time;
				}
			} else {
				unpeer(peer);
				return;
			}

			/*
			 * While we have a chance, if our system peer
			 * is zero or his stratum is greater than the
			 * last known stratum of this guy, make sure
			 * hpoll is clamped to the minimum before
			 * resetting the timer.
			 * If the peer has been unreachable for a while
			 * and we have a system peer who is at least his
			 * equal, we may want to ramp his polling interval
			 * up to avoid the useless traffic.
			 */
			if (sys_peer == 0
			    || sys_peer->stratum > peer->stratum) {
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
			off.l_ui = off.l_uf = 0;
			clock_filter(peer, &off, (s_fp)0, (u_fp)NTP_MAXDISPERSE);
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
	 * Finally, adjust the hpoll variable for special conditions.
	 */
	if (peer->hmode == MODE_BCLIENT)
		peer->hpoll = peer->ppoll;
	else if (peer->flags & FLAG_SYSPEER &&
	    peer->hpoll > sys_poll)
		peer->hpoll = max(peer->minpoll, sys_poll);

	/*
	 * Arrange for our next timeout.  hpoll will be less than
	 * maxpoll for sure.
	 */
	if (peer->event_timer.next != 0)
		/*
		 * Oops, someone did already.
		 */
		TIMER_DEQUEUE(&peer->event_timer);
	peer_timer = 1 << (int)max((u_char)min(peer->ppoll, peer->hpoll), peer->minpoll);
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
	U_LONG hiskeyid;
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
	 * Catch private mode packets.  Dump it if queries not allowed.
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
	 * See if we only accept limited number of clients
	 * from the net this guy is from.
	 * Note: the flag is determined dynamically within restrictions()
	 */
	if (restrict & RES_LIMITED) {
		extern U_LONG client_limit;

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
	 * Find the peer.  This will return a null if this guy
	 * isn't in the database.
	 */
	peer = findpeer(&rbufp->recv_srcadr, rbufp->dstadr);

	/*
	 * Check the length for validity, drop the packet if it is
	 * not as expected.
	 *
	 * If this is a client mode poll, go no further.  Send back
	 * his time and drop it.
	 *
	 * The scheme we use for authentication is this.  If we are
	 * running in non-authenticated mode, we accept both frames
	 * which are authenticated and frames which aren't, but don't
	 * authenticate.  We do record whether the frame had a mac field
	 * or not so we know what to do on output.
	 *
	 * If we are running in authenticated mode, we only trust frames
	 * which have authentication attached, which are validated and
	 * which are using one of our trusted keys.  We respond to all
	 * other pollers without saving any state.  If a host we are
	 * passively peering with changes his key from a trusted one to
	 * an untrusted one, we immediately unpeer with him, reselect
	 * the clock and treat him as an unmemorable client (this is
	 * a small denial-of-service hole I'll have to think about).
	 * If a similar event occurs with a configured peer we drop the
	 * frame and hope he'll revert to our key again.  If we get a
	 * frame which can't be authenticated with the given key, we
	 * drop it.  Either we disagree on the keys or someone is trying
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
		if (debug > 3)
		    printf("receive: pkt is %d octets, mac %d octets long, keyid %d\n",
			   rbufp->recv_length, has_mac, hiskeyid);
#endif
	} else if (rbufp->recv_length == LEN_PKT_NOMAC) {
		hiskeyid = 0;
		has_mac = 0;
	} else {
#ifdef DEBUG
		if (debug > 2)
			printf("receive: bad length %d (not > %d or == %d)\n",
			       rbufp->recv_length, LEN_PKT_MAC, LEN_PKT_NOMAC);
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
	if (PKT_VERSION(pkt->li_vn_mode) == NTP_OLDVERSION && hismode == 0) {
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
	 * If he included a mac field, decrypt it to see if it is authentic.
	 */
	is_authentic = 0;
	if (has_mac) {
		if (authhavekey(hiskeyid)) {
			if (authdecrypt(hiskeyid, (U_LONG *)pkt, LEN_PKT_NOMAC)) {
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
	 * If this is someone we don't remember from a previous association,
	 * dispatch him now.  Either we send something back quick, we
	 * ignore him, or we allocate some memory for him and let
	 * him continue.
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
				fast_xmit(rbufp, (int)hismode, is_authentic);
				return;
			}
			break;

		case MODE_PASSIVE:
#ifdef MCAST
			/* process the packet to determine the rt-delay */
#endif /* MCAST */
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
/*
			if ((restrict & RES_NOPEER) || !sys_bclient)
				return;
*/
			mymode = MODE_BCLIENT;
			break;
		}

		/*
		 * Okay, we're going to keep him around.  Allocate him
		 * some memory.
		 */
		peer = newpeer(&rbufp->recv_srcadr, rbufp->dstadr, mymode,
		    PKT_VERSION(pkt->li_vn_mode), NTP_MINDPOLL,
		    NTP_MAXPOLL, 0, hiskeyid);
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
			peer->keyid = hiskeyid;
			peer->flags |= FLAG_AUTHENABLE;
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
		 * in the past, but just failed the authentic test, report
		 * the event.
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
		if (!(peer->flags & FLAG_CONFIG)
		    || (peer->flags & FLAG_AUTHENABLE))
			trustable = 0;

		if (has_mac) {
			if (authistrusted(hiskeyid)) {
				if (is_authentic) {
					trustable = 1;
				} else {
					trustable = 0;
					peer->badauth++;
				}
			}
		}
	}

	/*
	 * Dispose of the packet based on our respective modes.  We
	 * don't drive this with a table, though we probably could.
	 */
	switch (peer->hmode) {
	case MODE_ACTIVE:
	case MODE_CLIENT:
		/*
		 * Active mode associations are configured.  If the data
		 * isn't trustable, ignore it and hope this guy brightens
		 * up.  Else accept any data we get and process it.
		 */
		switch (hismode) {
		case MODE_ACTIVE:
		case MODE_PASSIVE:
		case MODE_SERVER:
			process_packet(peer, pkt, &(rbufp->recv_time),
			    has_mac, trustable);
			break;

		case MODE_CLIENT:
			if (peer->hmode == MODE_ACTIVE)
				fast_xmit(rbufp, hismode, is_authentic);
			return;

		case MODE_BROADCAST:
			/*
			 * No good for us, we want real time.
			 */
			break;
		}
		break;

	case MODE_PASSIVE:
		/*
		 * Passive mode associations are (in the current
		 * implementation) always dynamic.  If we get an
		 * invalid header, break the connection.  I hate
		 * doing this since it seems like a waste.  Oh, well.
		 */
		switch (hismode) {
		case MODE_ACTIVE:
			if (process_packet(peer, pkt, &(rbufp->recv_time),
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
		 * Broadcast client pseudo-mode.  We accept both server
		 * and broadcast data.  Passive mode data is an error.
		 */
		switch (hismode) {
		case MODE_ACTIVE:
			/*
			 * This guy wants to give us real time when we've
			 * been existing on lousy broadcasts!  Create a
			 * passive mode association and do it that way,
			 * but keep the old one in case the packet turns
			 * out to be bad.
			 */
			peer2 = newpeer(&rbufp->recv_srcadr,
			    rbufp->dstadr, MODE_PASSIVE,
			    PKT_VERSION(pkt->li_vn_mode),
			    NTP_MINDPOLL, NTP_MAXPOLL, 0, hiskeyid);
			if (process_packet(peer2, pkt, &rbufp->recv_time,
			    has_mac, trustable) == 0) {
				/*
				 * Strange situation.  We've been receiving
				 * broadcasts from him which we liked, but
				 * we don't like his active mode stuff.
				 * Keep his old peer structure and send
				 * him some time quickly, we'll figure it
				 * out later.
				 */
				unpeer(peer2);
				fast_xmit(rbufp, (int)hismode, is_authentic);
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
	}
}


/*
 * process_packet - Packet Procedure, a la Section 3.4.3 of the specification.
 *	  	    Or almost, at least.  If we're in here we have a reasonable
 *		    expectation that we will be having a long term relationship
 *		    with this host.
 */
int
process_packet(peer, pkt, recv_ts, has_mac, trustable)
	register struct peer *peer;
	register struct pkt *pkt;
	l_fp *recv_ts;
	int has_mac;
	int trustable;			/* used as "valid header" */
{
	U_LONG t23_ui = 0, t23_uf = 0;
	U_LONG t10_ui, t10_uf;
	s_fp di, ei, p_dist, p_disp;
	l_fp ci, p_rec, p_xmt, p_org;
	int randomize;
	u_char ostratum, oreach;

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
		if ((p_rec.l_ui == 0 && p_rec.l_uf == 0) ||
		    (p_org.l_ui == 0 && p_org.l_uf == 0))
			peer->flash |= TEST3;	/* unsynchronized */
	} else {
		if (p_org.l_ui == 0 && p_org.l_uf == 0)
			peer->flash |= TEST3;   /* unsynchronized */
	}
	peer->org = p_xmt;	/* reuse byte-swapped pkt->xmt */
	peer->ppoll = pkt->ppoll;
		
	/*
	 * Call poll_update().  This will either start us, if the
	 * association is new, or drop the polling interval if the
	 * association is existing and ppoll has been reduced.
	 */
	poll_update(peer, peer->hpoll, randomize);

	/*
	 * Test for valid header (tests 5 through 8)
	 */
	if (trustable == 0)		/* test 5 */
		peer->flash |= TEST5;	/* authentication failed */
	if (PKT_LEAP(pkt->li_vn_mode) == LEAP_NOTINSYNC || /* test 6 */
	    p_xmt.l_ui < ntohl(pkt->reftime.l_ui) ||
	    p_xmt.l_ui >= (ntohl(pkt->reftime.l_ui) + NTP_MAXAGE)) {
		peer->seltooold++;	/* test 6 */
		peer->flash |= TEST6;	/* peer clock unsynchronized */
	}
	if (!(peer->flags & FLAG_CONFIG) &&  /* test 7 */
	    (PKT_TO_STRATUM(pkt->stratum) >= NTP_MAXSTRATUM ||
	    PKT_TO_STRATUM(pkt->stratum) > sys_stratum))
		peer->flash |= TEST7;	/* peer stratum out of bounds */
	if (p_dist >= NTP_MAXDISPERSE	/* test 8 */
	    || p_dist <= (-NTP_MAXDISPERSE)
	    || p_disp >= NTP_MAXDISPERSE) {
		peer->disttoolarge++;
		peer->flash |= TEST8;	/* delay/dispersion too big */
	}

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
	 * If running in a normal polled association, calculate the round
	 * trip delay (di) and the clock offset (ci).  We use the equations
	 * (reordered from those in the spec):
	 *
	 * d = (t2 - t3) - (t1 - t0)
	 * c = ((t2 - t3) + (t1 - t0)) / 2
	 *
	 * If running as a broadcast client, these change.  di becomes
	 * equal to two times our broadcast delay, while the offset
	 * becomes equal to:
	 *
	 * c = (t1 - t0) + estbdelay
	 */
	t10_ui = p_xmt.l_ui;	/* pkt->xmt == t1 */
	t10_uf = p_xmt.l_uf;
	M_SUB(t10_ui, t10_uf, peer->rec.l_ui, peer->rec.l_uf); /*peer->rec==t0*/

	if (PKT_MODE(pkt->li_vn_mode) != MODE_BROADCAST) {
		t23_ui = p_rec.l_ui;	/* pkt->rec == t2 */
		t23_uf = p_rec.l_uf;
		M_SUB(t23_ui, t23_uf, p_org.l_ui, p_org.l_uf); /*pkt->org==t3*/
	}

	/* now have (t2 - t3) and (t0 - t1).  Calculate (ci), (di) and (ei) */
	ci.l_ui = t10_ui;
	ci.l_uf = t10_uf;
	ei = (FP_SECOND >> (-(int)sys_precision));

	/*
	 * If broadcast mode, time of last reception has been fiddled
	 * to p_org, rather than originate timestamp. We use this to
	 * augment dispersion and previously calcuated estbdelay as
	 * the delay. We know NTP_SKEWFACTOR == 16, which accounts for
	 * the simplified ei calculation.
	 */
	if (PKT_MODE(pkt->li_vn_mode) == MODE_BROADCAST) {
		M_ADDUF(ci.l_ui, ci.l_uf, peer->estbdelay >> 1);
		di = MFPTOFP(0, peer->estbdelay);
		ei += peer->rec.l_ui - p_org.l_ui;
	} else {
		M_ADD(ci.l_ui, ci.l_uf, t23_ui, t23_uf);
		M_RSHIFT(ci.l_i, ci.l_uf);
		M_SUB(t23_ui, t23_uf, t10_ui, t10_uf);
		di = MFPTOFP(t23_ui, t23_uf);
		ei += peer->rec.l_ui - p_org.l_ui;
	}
#ifdef DEBUG
	if (debug > 3)
		printf("offset: %s, delay %s, error %s\n",
		       lfptoa(&ci, 9), fptoa(di, 4), fptoa(ei, 4));
#endif
	if (di >= NTP_MAXDISPERSE || di <= (-NTP_MAXDISPERSE)
	    || ei >= NTP_MAXDISPERSE) {	/* test 4 */
		peer->bogusdelay++;
		peer->flash |= TEST4;	/* delay/dispersion too big */
	}

	/*
	 * If the packet data is invalid (tests 1 through 4), exit
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
	 * This one is valid.  Mark it so, give it to clock_filter(),
	 */
	clock_filter(peer, &ci, di, (u_fp)ei);

	/*
	 * If this guy was previously unreachable, report him
	 * reachable.
	 * Note we do this here so that the peer values we return are
	 * the updated ones.
	 */
	if (oreach == 0) {
		report_event(EVNT_REACH, peer);
#ifdef DEBUG
		if (debug)
			printf("proto: peer reach %d\n", peer->minpoll);
#endif /* DEBUG */
	}

	/*
	 * Now update the clock.
	 */
	clock_update(peer);
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

	record_peer_stats(&peer->srcadr, ctlpeerstatus(peer), &peer->offset,
	    peer->delay, peer->dispersion);

	/*
	 * Call the clock selection algorithm to see
	 * if this update causes the peer to change.
	 */
	clock_select();

	/*
	 * Quit if this peer isn't the system peer.  Other peers
	 * used in the combined offset are not allowed to set
	 * system variables or update the clock.
	 */
	if (peer != sys_peer)
		return;

	/*
	 * Quit if the sys_peer is too far away.
	 */
	if (peer->synch >= NTP_MAXDISTANCE)
		return;

	/*
	 * Update the system state
	 */
	oleap = sys_leap;
	ostratum = sys_stratum;
	/*
	 * get leap value (usually the peer leap unless overridden by local configuration)
	 */
	sys_leap = leap_actual(peer->leap & leap_mask);
	/*
	 * N.B. peer->stratum was guaranteed to be less than
	 * NTP_MAXSTRATUM by the receive procedure.
	 * We assume peer->update == sys_clock because
	 * clock_filter was called right before this function.
	 * If the pps signal is in control, the system variables are
	 * set in the ntp_loopfilter.c module.
	 */
	if (!pps_control) {
		sys_stratum = peer->stratum + 1;
		d = peer->delay;
		if (d < 0)
			d = -d;
	 	sys_rootdelay = peer->rootdelay + d;
		sys_maxd = peer->dispersion + peer->selectdisp;
		d = peer->soffset;
		if (d < 0)
			d = -d;
		d += sys_maxd;
		if (!peer->flags & FLAG_REFCLOCK && d < NTP_MINDISPERSE)
			d = NTP_MINDISPERSE;
		sys_rootdispersion = peer->rootdispersion + d;
	}
	
	/*
	 * Hack for reference clocks.  Sigh.  This is the
	 * only real silly part, though, so the analogy isn't
	 * bad.
	 */
	if (peer->flags & FLAG_REFCLOCK && peer->stratum == STRATUM_REFCLOCK)
		sys_refid = peer->refid;
	else {
		if (pps_control)
		    memmove((char *)&sys_refid, PPSREFID, 4);
		else
		    sys_refid = peer->srcadr.sin_addr.s_addr;
	}

	/*
	 * Report changes.  Note that we never sync to
	 * an unsynchronized host.
	 */
	if (oleap == LEAP_NOTINSYNC)
		report_event(EVNT_SYNCCHG, (struct peer *)0);
	else if (ostratum != sys_stratum)
		report_event(EVNT_PEERSTCHG, (struct peer *)0);

	sys_reftime = peer->rec;
	sys_refskew.l_i = 0; sys_refskew.l_f = NTP_SKEWINC;

	switch (local_clock(&sys_offset, peer)) {
	case -1:
		/*
		 * Clock is too screwed up.  Just exit for now.
		 */
		report_event(EVNT_SYSFAULT, (struct peer *)0);
		exit(1);
		/*NOTREACHED*/
	case 0:
		/*
		 * Clock was slewed.  Continue on normally.
		 */
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
	if (sys_stratum > 1)
		sys_refid = peer->srcadr.sin_addr.s_addr;
	else {
		if (peer->flags & FLAG_REFCLOCK)
			sys_refid = peer->refid;
		else
			memmove((char *)&sys_refid, PPSREFID, 4);
	}
}



/*
 * poll_update - update peer poll interval.  See Section 3.4.8 of the spec.
 */
void
poll_update(peer, new_hpoll, randomize)
	struct peer *peer;
	unsigned int new_hpoll;
	int randomize;
{
	register struct event *evp;
	register U_LONG new_timer;
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
	if (peer->flags & FLAG_REFCLOCK || peer->hmode == MODE_BROADCAST)
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
	newpoll = max((u_char)min(peer->ppoll, peer->hpoll), peer->minpoll);
	if (randomize == POLL_MAKERANDOM ||
	    (randomize == POLL_RANDOMCHANGE && newpoll != oldpoll))
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
			/*
			 * We used to drop all unconfigured pollers here.
			 * The problem with doing this is that if your best
			 * time source is unconfigured (there are reasons
			 * for doing this) and you drop him, he may not
			 * get around to polling you for a long time.  Hang
			 * on to everyone, dropping their polling intervals
			 * to the minimum.
			 */
			peer_clear(peer);
		}

	/*
	 * Clear sys_peer.  We'll sync to one later.
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
	register int i;
	register u_char *ord;
	register s_fp sample_distance, sample_soffset, skew;
	s_fp distance[NTP_SHIFT];

#ifdef DEBUG
	if (debug)
		printf("clock_filter(%s, %s, %s, %s)\n",
		    ntoa(&peer->srcadr), lfptoa(sample_offset, 6),
		    fptoa(sample_delay, 5), ufptoa(sample_error, 5));
#endif

	/*
	 * Update sample errors and calculate distances.
	 * We know NTP_SKEWFACTOR == 16
	 */
	skew = sys_clock - peer->update;
	peer->update = sys_clock;
	for (i = 0; i < NTP_SHIFT; i++) {
		distance[i] = peer->filter_error[i];
		if (peer->filter_error[i] < NTP_MAXDISPERSE) {
			peer->filter_error[i] += skew;
			distance[i] += (peer->filter_delay[i] >> 1);
		}
	}

	/*
	 * We keep a sort by distance of the current contents of the
	 * shift registers.  We update this by (1) removing the
	 * register we are going to be replacing from the sort, and
	 * (2) reinserting it based on the new distance value.
	 */
	ord = peer->filter_order;
	sample_distance = sample_error + (sample_delay >> 1);
	sample_soffset = LFPTOFP(sample_offset);

	for (i = 0; i < NTP_SHIFT-1; i++)	/* find old value */
		if (ord[i] == peer->filter_nextpt)
			break;
	for ( ; i < NTP_SHIFT-1; i++)	/* i is current, move everything up */
		ord[i] = ord[i+1];
	/* Here, last slot in ord[] is empty */

	if (sample_error >= NTP_MAXDISPERSE)
		/*
		 * Last slot for this guy.
		 */
		i = NTP_SHIFT-1;
	else {
		register int j;
		register u_fp *errorp;

		errorp = peer->filter_error;
		/*
		 * Find where he goes in, then shift everyone else down
		 */
		for (i = 0; i < NTP_SHIFT-1; i++)
			if (errorp[ord[i]] >= NTP_MAXDISPERSE
			    || sample_distance <= distance[ord[i]])
				break;

		for (j = NTP_SHIFT-1; j > i; j--)
			ord[j] = ord[j-1];
	}
	ord[i] = peer->filter_nextpt;

	/*
	 * Got everything in order.  Insert sample in current register
	 * and increment nextpt.
	 */
	peer->filter_delay[peer->filter_nextpt] = sample_delay;
	peer->filter_offset[peer->filter_nextpt] = *sample_offset;
	peer->filter_soffset[peer->filter_nextpt] = sample_soffset;
	peer->filter_error[peer->filter_nextpt] = sample_error;
	distance[peer->filter_nextpt] = sample_distance;
	peer->filter_nextpt++;
	if (peer->filter_nextpt >= NTP_SHIFT)
		peer->filter_nextpt = 0;
	
	/*
	 * Now compute the dispersion, and assign values to delay and
	 * offset.  If there are no samples in the register, delay and
	 * offset are not touched and dispersion is set to the maximum.
	 */
	if (peer->filter_error[ord[0]] >= NTP_MAXDISPERSE) {
		peer->dispersion = NTP_MAXDISPERSE;
	} else {
		register s_fp d;

		peer->delay = peer->filter_delay[ord[0]];
		peer->offset = peer->filter_offset[ord[0]];
		peer->soffset = LFPTOFP(&peer->offset);
		peer->dispersion = peer->filter_error[ord[0]];
		for (i = 1; i < NTP_SHIFT; i++) {
			if (peer->filter_error[ord[i]] >= NTP_MAXDISPERSE)
				d = NTP_MAXDISPERSE;
			else {
				d = peer->filter_soffset[ord[i]]
				    - peer->filter_soffset[ord[0]];
				if (d < 0)
					d = -d;
				if (d > NTP_MAXDISPERSE)
					d = NTP_MAXDISPERSE;
			}
			/*
			 * XXX This *knows* NTP_FILTER is 1/2
			 */
			peer->dispersion += (u_fp)(d) >> i;
		}
		/*
		 * Calculate synchronization distance backdated to
		 * sys_lastselect (clock_select will fix it).
		 * We know NTP_SKEWFACTOR == 16
		 */
		d = peer->delay;
		if (d < 0)
			d = -d;
		d += peer->rootdelay;
		peer->synch = (d>>1)
			      + peer->rootdispersion + peer->dispersion
			      - (sys_clock - sys_lastselect);
	}
	/*
	 * We're done
	 */
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
	/* XXX correct? */
	s_fp low = 0x7ffffff;
	s_fp high = 0x00000000;
	u_fp synch[NTP_MAXCLOCK], error[NTP_MAXCLOCK];
	struct peer *osys_peer;
	static int list_alloc = 0;
	static struct endpoint *endpoint;
	static int *index;
	static struct peer **peer_list;
	static int endpoint_size = 0, index_size = 0, peer_list_size = 0;

#ifdef DEBUG
	if (debug > 1)
		printf("clock_select()\n");
#endif

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
			 * Update synch distance (NTP_SKEWFACTOR == 16)
			 */
			peer->synch += (sys_clock - sys_lastselect);

			if (peer->reach == 0)
				continue;	/* unreachable */
			if (peer->stratum > 1 &&
			    peer->refid == peer->dstadr->sin.sin_addr.s_addr)
				continue;	/* sync loop */
			if (peer->stratum >= NTP_MAXSTRATUM ||
	    		    peer->stratum > sys_stratum)
				continue;	/* bad stratum  */

			if (peer->dispersion >= NTP_MAXDISPERSE) {
				peer->seldisptoolarge++;
				continue;	/* too noisy or broken */
			}
			if (peer->org.l_ui < peer->reftime.l_ui) {
				peer->selbroken++;
				continue;	/* very broken host */
			}

			/*
			 * This one seems sane.
			 */
			peer->was_sane = 1;
			peer_list[nlist++] = peer;

			/*
			 * Insert each interval endpoint on the sorted list.
			 */
			e = peer->soffset + peer->synch;	/* Upper end */
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
	allow = nlist;	/* falsetickers assumed */
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
	if ((allow << 1) >= nlist) {
		if (debug)
			printf("clock_select: no intersection\n");
		if (sys_peer != 0)
			report_event(EVNT_PEERSTCHG, (struct peer *)0);
		sys_peer = 0;
		sys_stratum = STRATUM_UNSPEC;
		return;
	}

#ifdef DEBUG
	if (debug > 2)
		printf("select: low %s high %s\n", fptoa(low, 6),
		       fptoa(high, 6));
#endif

	/*
	 * Clustering algorithm. Process intersection list to discard
	 * outlyers. First, construct candidate list in cluster order.
	 * Cluster order is determined by the sum of peer
	 * synchronization distance plus scaled stratum.
	 */

	j = 0;
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		if (peer->soffset < low || high < peer->soffset)
			continue;
		peer->correct = 1;
		d = peer->synch + ((U_LONG)peer->stratum << NTP_DISPFACTOR);
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
	 * Now, prune outlyers by root dispersion.
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

			for (j = nlist - 1; j >= 0; j--) {
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
	 * What remains is a list of less than NTP_MINCLOCK peers.
	 * First record their order, then choose a peer.  If the
	 * head of the list has a lower stratum than sys_peer
	 * choose him right off.  If not, see if sys_peer is in
	 * the list.  If so, keep him.  If not, take the top of
	 * the list anyway. Also, clamp the polling intervals.
	 */
	osys_peer = sys_peer;
	for (i = nlist - 1; i >= 0; i--) {
		if (peer_list[i]->flags & FLAG_PREFER)
		    sys_peer = peer_list[i];
		peer_list[i]->select = i + 1;
		peer_list[i]->flags |= FLAG_SYSPEER;
		poll_update(peer_list[i], peer_list[i]->hpoll,
		    POLL_RANDOMCHANGE);
	}
	if (sys_peer == 0 || sys_peer->stratum > peer_list[0]->stratum) {
		sys_peer = peer_list[0];
	} else {
		for (i = 1; i < nlist; i++)
			if (peer_list[i] == sys_peer)
				break;
		if (i >= nlist)
			sys_peer = peer_list[0];
	}

	/*
	 * If we got a new system peer from all of this, report the event.
	 */
	if (osys_peer != sys_peer)
		report_event(EVNT_PEERSTCHG, (struct peer *)0);

	/*
	 * Combine the offsets of the survivors to form a weighted
	 * offset.
	 */
	clock_combine(peer_list, nlist);
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
	 * Sort peers by cluster distance as in the outlyer algorithm. If
	 * the preferred peer is found, use its offset only.
	 */
	k = 0;
	for (i = 0; i < npeers; i++) {
		if (peers[i]->stratum > sys_peer->stratum) continue;
		if (peers[i]->flags & FLAG_PREFER) {
			sys_offset = peers[i]->offset;
			pps_update = current_time;
#ifdef DEBUG
			printf("combine: prefer offset %s\n",
			    lfptoa(&sys_offset, 6));
#endif
			return;
		}
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

#ifdef DEBUG
	if (debug) {
	printf("combine: offset %s\n", lfptoa(&sys_offset, 6));
	}
#endif

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
	xpkt.rootdispersion = HTONS_FP(sys_rootdispersion +
				       (FP_SECOND >> (-(int)sys_precision)) +
				       LFPTOFP(&sys_refskew));
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
		sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, &xpkt,
			LEN_PKT_NOMAC + maclen);
	} else {
		/*
		 * Get xmt timestamp, then send it without mac field
		 */
		get_systime(&xmt_ts);
		HTONL_FP(&xmt_ts, &xpkt.xmt);
		sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, &xpkt,
		    LEN_PKT_NOMAC);
	}
}

/* Find the precision of the system clock by watching how the current time
 * changes as we read it repeatedly.
 *
 * struct timeval is only good to 1us, which may cause problems as machines
 * get faster, but until then the logic goes:
 *
 * If a machine has precision (i.e. accurate timing info) > 1us, then it will
 * probably use the "unused" low order bits as a counter (to force time to be
 * a strictly increaing variable), incrementing it each time any process
 * requests the time [[ or maybe time will stand still ? ]].
 *
 * SO: the logic goes:
 *
 *	IF	the difference from the last time is "small" (< MINSTEP)
 *	THEN	this machine is "counting" with the low order bits
 *	ELIF	this is not the first time round the loop
 *	THEN	this machine *WAS* counting, and has now stepped
 *	ELSE	this machine has precision < time to read clock
 *
 * SO: if it exits on the first loop, assume "full accuracy" (1us)
 *     otherwise, take the log2(observered difference, rounded UP)
 *
 * MINLOOPS > 1 ensures that even if there is a STEP between the initial call
 * and the first loop, it doesn't stop too early.
 * Making it even greater allows MINSTEP to be reduced, assuming that the
 * chance of MINSTEP-1 other processes getting in and calling gettimeofday
 * between this processes's calls.
 * Reducing MINSTEP may be necessary as this sets an upper bound for the time
 * to actually call gettimeofday.
 */

#define	DUSECS	1000000	/* us's as returned by gettime */
#define	HUSECS	(1024*1024) /* Hex us's -- used when shifting etc */
#define	MINSTEP	5	/* some systems increment uS on each call */
			/* Don't use "1" as some *other* process may read too*/
			/*We assume no system actually *ANSWERS* in this time*/
#define	MAXLOOPS DUSECS	/* if no STEP in a complete second, then FAST machine*/
#define MINLOOPS 2	/* ensure at least this many loops */

int default_get_precision()
{
	struct timeval tp;
	struct timezone tzp;
	long last;
	int i;
	long diff;
	long val;
	int minsteps = 2;	/* need at least this many steps */

	GETTIMEOFDAY(&tp, &tzp);
	last = tp.tv_usec;
	for (i = - --minsteps; i< MAXLOOPS; i++) {
		gettimeofday(&tp, &tzp);
		diff = tp.tv_usec - last;
		if (diff < 0) diff += DUSECS;
		if (diff > MINSTEP) if (minsteps-- <= 0) break;
		last = tp.tv_usec;
	}

	syslog(LOG_INFO, "precision calculation given %dus after %d loop%s",
		diff, i, (i==1) ? "" : "s");

	diff = (diff*3) / 2;	/* round it up 1.5 is approx sqrt(2) */
	if (i >= MAXLOOPS)	diff = 1; /* No STEP, so FAST machine */
	if (i == 0)		diff = 1; /* time to read clock >= precision */
	for (i=0, val=HUSECS; val>0; i--, val >>= 1) if (diff >= val) return i;
	return DEFAULT_SYS_PRECISION /* Something's BUST, so lie ! */;
}

/*
 * init_proto - initialize the protocol module's data
 */
void
init_proto()
{
	l_fp dummy;

	/*
	 * Fill in the sys_* stuff.  Default is don't listen
	 * to broadcasting, don't authenticate.
	 */
	sys_leap = LEAP_NOTINSYNC;
	sys_stratum = STRATUM_UNSPEC;
	sys_precision = (s_char)default_get_precision();
	sys_rootdelay = 0;
	sys_rootdispersion = 0;
	sys_refid = 0;
	sys_reftime.l_ui = sys_reftime.l_uf = 0;
	sys_refskew.l_i = NTP_MAXSKEW; sys_refskew.l_f = 0;
	sys_peer = 0;
	sys_poll = NTP_MINPOLL;
	get_systime(&dummy);
	sys_lastselect = sys_clock;

	sys_bclient = 0;
	sys_bdelay = DEFBROADDELAY;

	sys_authenticate = 0;

	sys_stattime = 0;
	sys_badstratum = 0;
	sys_oldversionpkt = 0;
	sys_newversionpkt = 0;
	sys_badlength = 0;
	sys_unknownversion = 0;
	sys_processed = 0;
	sys_badauth = 0;

	syslog(LOG_NOTICE, "default precision is initialized to 2**%d", sys_precision);
}


/*
 * proto_config - configure the protocol module
 */
void
proto_config(item, value)
	int item;
	U_LONG value;
{
	/*
	 * Figure out what he wants to change, then do it
	 */
	switch (item) {
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
		 * Add multicast group address
		 */
		if (!sys_bclient) {
			sys_bclient = 1;
			io_setbclient();
		}
#ifdef MCAST
		io_multicast_add(value);
#endif /* MCAST */
		break;

	case PROTO_MULTICAST_DEL:
		/*
		 * Delete multicast group address
		 */
#ifdef MCAST
		io_multicast_del(value);
#endif /* MCAST */
		break;
	
	case PROTO_PRECISION:
		/*
		 * Set system precision
		 */
		sys_precision = (s_char)value;
		break;
	
	case PROTO_BROADDELAY:
		/*
		 * Set default broadcast delay
		 */
		sys_bdelay = ((value) + 0x00000800) & 0xfffff000;
		break;
	
	case PROTO_AUTHENTICATE:
		/*
		 * Specify the use of authenticated data
		 */
		sys_authenticate = (int)value;
		break;


	case PROTO_AUTHDELAY:
		/*
		 * Provide an authentication delay value.  Round it to
		 * the microsecond.  This is crude.
		 */
		sys_authdelay = ((value) + 0x00000800) & 0xfffff000;
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
