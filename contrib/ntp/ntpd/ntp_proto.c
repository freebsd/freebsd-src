/*
 * ntp_proto.c - NTP version 4 protocol machinery
 *
 * $FreeBSD$
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"
#include "ntp_control.h"
#include "ntp_string.h"

#if defined(VMS) && defined(VMS_LOCALUNIT)	/*wjm*/
#include "ntp_refclock.h"
#endif

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/sysctl.h>
#endif

/*
 * System variables are declared here.	See Section 3.2 of the
 * specification.
 */
u_char	sys_leap;		/* system leap indicator */
u_char	sys_stratum;		/* stratum of system */
s_char	sys_precision;		/* local clock precision */
double	sys_rootdelay;		/* distance to current sync source */
double	sys_rootdispersion;	/* dispersion of system clock */
u_int32 sys_refid;		/* reference source for local clock */
static	double sys_offset;	/* current local clock offset */
l_fp	sys_reftime;		/* time we were last updated */
struct	peer *sys_peer; 	/* our current peer */
u_long	sys_automax;		/* maximum session key lifetime */

/*
 * Nonspecified system state variables.
 */
int	sys_bclient;		/* we set our time to broadcasts */
double	sys_bdelay; 		/* broadcast client default delay */
int	sys_authenticate;	/* requre authentication for config */
l_fp	sys_authdelay;		/* authentication delay */
static	u_long sys_authdly[2]; 	/* authentication delay shift reg */
static	u_char leap_consensus;	/* consensus of survivor leap bits */
static	double sys_maxd; 	/* select error (squares) */
static	double sys_epsil;	/* system error (squares) */
u_long	sys_private;		/* private value for session seed */
int	sys_manycastserver;	/* 1 => respond to manycast client pkts */

/*
 * Statistics counters
 */
u_long	sys_stattime;		/* time when we started recording */
u_long	sys_badstratum; 	/* packets with invalid stratum */
u_long	sys_oldversionpkt;	/* old version packets received */
u_long	sys_newversionpkt;	/* new version packets received */
u_long	sys_unknownversion;	/* don't know version packets */
u_long	sys_badlength;		/* packets with bad length */
u_long	sys_processed;		/* packets processed */
u_long	sys_badauth;		/* packets dropped because of auth */
u_long	sys_limitrejected;	/* pkts rejected due to client count per net */

static	double	root_distance	P((struct peer *));
static	double	clock_combine	P((struct peer **, int));
static	void	peer_xmit	P((struct peer *));
static	void	fast_xmit	P((struct recvbuf *, int, u_long));
static	void	clock_update	P((void));
int	default_get_precision	P((void));
#ifdef MD5
static	void	make_keylist	P((struct peer *));
#endif /* MD5 */

/*
 * transmit - Transmit Procedure. See Section 3.4.2 of the
 *	specification.
 */
void
transmit(
	struct peer *peer	/* peer structure pointer */
	)
{
	int hpoll;

	hpoll = peer->hpoll;
	if (peer->burst == 0) {
		u_char oreach;

		/*
		 * Determine reachability and diddle things if we
		 * haven't heard from the host for a while. If the peer
		 * is not configured and not likely to stay around,
		 * we exhaust it.
		 */
		oreach = peer->reach;
		if (oreach & 0x01)
			peer->valid++;
		if (oreach & 0x80)
			peer->valid--;
		if (!(peer->flags & FLAG_CONFIG) && peer->valid >
		    NTP_SHIFT / 2 && (peer->reach & 0x80) &&
		    peer->status < CTL_PST_SEL_SYNCCAND)
			peer->reach = 0;
		peer->reach <<= 1;
		if (peer->reach == 0) {

			/*
			 * If this is an uncofigured association and
			 * has become unreachable, demobilize it.
			 */
			if (oreach != 0) {
				report_event(EVNT_UNREACH, peer);
				peer->timereachable = current_time;
				peer_clear(peer);
				if (!(peer->flags & FLAG_CONFIG)) {
					unpeer(peer);
					return;
				}
			}

			/*
			 * We would like to respond quickly when the
			 * peer comes back to life. If the probes since
			 * becoming unreachable are less than
			 * NTP_UNREACH, clamp the poll interval to the
			 * minimum. In order to minimize the network
			 * traffic, the interval gradually ramps up the
			 * the maximum after that.
			 */
			peer->ppoll = peer->maxpoll;
			if (peer->unreach < NTP_UNREACH) {
				if (peer->hmode == MODE_CLIENT)
					peer->unreach++;
				hpoll = peer->minpoll;
			} else {
				hpoll++;
			}
			if (peer->flags & FLAG_BURST)
				peer->burst = 2;

		} else {

			/*
			 * Here the peer is reachable. If there is no
			 * system peer or if the stratum of the system
			 * peer is greater than this peer, clamp the
			 * poll interval to the minimum. If less than
			 * two samples are in the reachability register,
			 * reduce the interval; if more than six samples
			 * are in the register, increase the interval.
			 */
			peer->unreach = 0;
			if (sys_peer == 0)
				hpoll = peer->minpoll;
			else if (sys_peer->stratum > peer->stratum)
				hpoll = peer->minpoll;
			if ((peer->reach & 0x03) == 0) {
				clock_filter(peer, 0., 0., MAXDISPERSE);
				clock_select();
			}
			if (peer->valid <= 2)
				hpoll--;
			else if (peer->valid >= NTP_SHIFT - 2)
				hpoll++;
			if (peer->flags & FLAG_BURST)
				peer->burst = NTP_SHIFT;
		}
	} else {
		peer->burst--;
		if (peer->burst == 0) {
			if (peer->flags & FLAG_MCAST2) {
				peer->flags &= ~FLAG_BURST;
				peer->hmode = MODE_BCLIENT;
			}
			clock_select();
			poll_update(peer, hpoll);
			return;
		}
	}

	/*
	 * We need to be very careful about honking uncivilized time. If
	 * not operating in broadcast mode, honk in all except broadcast
	 * client mode. If operating in broadcast mode and synchronized
	 * to a real source, honk except when the peer is the local-
	 * clock driver and the prefer flag is not set. In other words,
	 * in broadcast mode we never honk unless known to be
	 * synchronized to real time.
	 */
	if (peer->hmode != MODE_BROADCAST) {
		if (peer->hmode != MODE_BCLIENT)
			peer_xmit(peer);
	} else if (sys_peer != 0 && sys_leap != LEAP_NOTINSYNC) {
		if (!(sys_peer->refclktype == REFCLK_LOCALCLOCK &&
		    !(sys_peer->flags & FLAG_PREFER)))
			peer_xmit(peer);
	}
	peer->outdate = current_time;
	poll_update(peer, hpoll);
}

/*
 * receive - Receive Procedure.  See section 3.4.3 in the specification.
 */
void
receive(
	struct recvbuf *rbufp
	)
{
	register struct peer *peer;
	register struct pkt *pkt;
	int hismode;
	int oflags;
	int restrict_mask;
	int has_mac;			/* has MAC field */
	int authlen;			/* length of MAC field */
	int is_authentic;		/* cryptosum ok */
	int is_mystic;			/* session key exists */
	int is_error;			/* parse error */
/*	u_long pkeyid; */
	u_long skeyid, tkeyid;
	struct peer *peer2;
	int retcode = AM_NOMATCH;

	/*
	 * Monitor the packet and get restrictions
	 */
	ntp_monitor(rbufp);
	restrict_mask = restrictions(&rbufp->recv_srcadr);
#ifdef DEBUG
	if (debug > 1)
		printf("receive: from %s restrict %02x\n",
		    ntoa(&rbufp->recv_srcadr), restrict_mask);
#endif
	if (restrict_mask & RES_IGNORE)
		return;

	/*
	 * Discard packets with invalid version number.
	 */
	pkt = &rbufp->recv_pkt;
	if (PKT_VERSION(pkt->li_vn_mode) >= NTP_VERSION)
		sys_newversionpkt++;
	else if (PKT_VERSION(pkt->li_vn_mode) >= NTP_OLDVERSION)
		sys_oldversionpkt++;
	else {
		sys_unknownversion++;
		return;
	}

	/*
	 * Restrict control/private mode packets. Note that packet
	 * length has to be checked in the control/private mode protocol
	 * module.
	 */
	if (PKT_MODE(pkt->li_vn_mode) == MODE_PRIVATE) {
		if (restrict_mask & RES_NOQUERY)
		    return;
		process_private(rbufp, ((restrict_mask &
		    RES_NOMODIFY) == 0));
		return;
	}
	if (PKT_MODE(pkt->li_vn_mode) == MODE_CONTROL) {
		if (restrict_mask & RES_NOQUERY)
		    return;
		process_control(rbufp, restrict_mask);
		return;
	}

	/*
	 * Restrict revenue packets.
	 */
	if (restrict_mask & RES_DONTSERVE)
		return;

        /*
	 * See if we only accept limited number of clients from the net
	 * this guy is from. Note: the flag is determined dynamically
	 * within restrictions()
	 */
	if (restrict_mask & RES_LIMITED) {
		sys_limitrejected++;
                return;
        }

	/*
	 * If we are not a broadcast client, ignore broadcast packets.
	 */
	if ((PKT_MODE(pkt->li_vn_mode) == MODE_BROADCAST &&
	    !sys_bclient))
		return;

	/*
	 * This is really awful ugly. We figure out whether an extension
	 * field is present and then measure the MAC size. If the number
	 * of words following the packet header is less than or equal to
	 * 5, no extension field is present and these words constitute
	 * the MAC. If the number of words is greater than 5, an
	 * extension field is present and the first word contains the
	 * length of the extension field and the MAC follows that.
	 */
	has_mac = 0;
/*	pkeyid = 0; */
	skeyid = tkeyid = 0;
	authlen = LEN_PKT_NOMAC;
	has_mac = rbufp->recv_length - authlen;
	if (has_mac <= 5 * sizeof(u_int32)) {
		skeyid = (u_long)ntohl(pkt->keyid1) & 0xffffffff;
	} else {
		authlen += (u_long)ntohl(pkt->keyid1) & 0xffffffff;
		has_mac = rbufp->recv_length - authlen;
		if (authlen <= 0) {
			sys_badlength++;
			return;
		}

		/*
		 * Note that keyid3 is actually the key ident of the
		 * MAC itself.
		 */
/* 		pkeyid = (u_long)ntohl(pkt->keyid2) & 0xffffffff; */
		skeyid = tkeyid = (u_long)ntohl(pkt->keyid3) &
		    0xffffffff;
	}

	/*
	 * Figure out his mode and validate it.
	 */
	hismode = (int)PKT_MODE(pkt->li_vn_mode);
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
			hismode != MODE_BROADCAST)
			return;
	}

	/*
	 * If he included a mac field, decrypt it to see if it is
	 * authentic.
	 */
	is_authentic = is_mystic = 0;
	if (has_mac == 0) {
#ifdef DEBUG
		if (debug)
			printf("receive: at %ld from %s mode %d\n",
				current_time, ntoa(&rbufp->recv_srcadr),
				hismode);
#endif
	} else {
		is_mystic = authistrusted(skeyid);
#ifdef MD5
		if (skeyid > NTP_MAXKEY && !is_mystic) {

			/*
			 * For multicast mode, generate the session key
			 * and install in the key cache. For client
			 * mode, generate the session key for the
			 * unicast address. For server mode, the session
			 * key should already be in the key cache, since
			 * it was generated when the last request was
			 * sent.
			 */
			if (hismode == MODE_BROADCAST) {
				tkeyid = session_key(
					ntohl((&rbufp->recv_srcadr)->sin_addr.s_addr),
					ntohl(rbufp->dstadr->bcast.sin_addr.s_addr),
					skeyid, (u_long)(4 * (1 << pkt->ppoll)));
			} else if (hismode != MODE_SERVER) {
				tkeyid = session_key(
					ntohl((&rbufp->recv_srcadr)->sin_addr.s_addr),
					ntohl(rbufp->dstadr->sin.sin_addr.s_addr),
					skeyid, (u_long)(4 * (1 << pkt->ppoll)));
			}

		}
#endif /* MD5 */

		/*
		 * Compute the cryptosum. Note a clogging attack may
		 * succceed in bloating the key cache.
		 */
		if (authdecrypt(skeyid, (u_int32 *)pkt, authlen,
		    has_mac))
			is_authentic = 1;
		else
			sys_badauth++;
#ifdef DEBUG
		if (debug)
			printf(
				"receive: at %ld %s mode %d keyid %08lx mac %d auth %d\n",
				current_time, ntoa(&rbufp->recv_srcadr),
				hismode, skeyid, has_mac, is_authentic);
#endif
	}

	/*
	 * Find the peer.  This will return a null if this guy isn't in
	 * the database.
	 */
	peer = findpeer(&rbufp->recv_srcadr, rbufp->dstadr, rbufp->fd,
		hismode, &retcode);
	/*
	 * The new association matching rules are driven by a table
	 * specified in ntp.h. We have replaced the *default* behaviour
	 * of replying to bogus packets in server mode in this version.
	 * A packet must now match an association in order to be
	 * processed. In the event that no association exists, then an
	 * association is mobilized if need be. Two different
	 * associations can be mobilized a) passive associations b)
	 * client associations due to broadcasts or manycasts.
	 */
	is_error = 0;
	switch (retcode) {
	case AM_FXMIT:

		/*
		 * If the client is configured purely as a broadcast
		 * client and not as an manycast server, it has no
		 * business being a server. Simply go home. Otherwise,
		 * send a MODE_SERVER response and go home. Note that we
		 * don't do a authentication check here, since we can't
		 * set the system clock; but, we do set the key ID to
		 * zero to tell the caller about this.
		 */
		if (!sys_bclient || sys_manycastserver) {
			if (is_authentic)
				fast_xmit(rbufp, MODE_SERVER, skeyid);
			else
				fast_xmit(rbufp, MODE_SERVER, 0);
		}

		/*
		 * We can't get here if an association is mobilized, so
		 * just toss the key, if appropriate.
		 */
		if (!is_mystic && skeyid > NTP_MAXKEY)
			authtrust(skeyid, 0);
			return;

	case AM_MANYCAST:

		/*
		 * This could be in response to a multicast packet sent
		 * by the "manycast" mode association. Find peer based
		 * on the originate timestamp in the packet. Note that
		 * we don't mobilize a new association, unless the
		 * packet is properly authenticated. The response must
		 * be properly authenticated and it's darn funny of the
		 * manycaster isn't around now. 
		 */
		if ((sys_authenticate && !is_authentic)) {
			is_error = 1;
			break;
		}
		peer2 = (struct peer *)findmanycastpeer(&pkt->org);
		if (peer2 == 0) {
			is_error = 1;
			break;
		}

		/*
		 * Create a new association and copy the peer variables
		 * to it. If something goes wrong, carefully pry the new
		 * association away and return its marbles to the candy
		 * store.
		*/
		peer = newpeer(&rbufp->recv_srcadr, rbufp->dstadr,
		    MODE_CLIENT, PKT_VERSION(pkt->li_vn_mode),
		    NTP_MINDPOLL, NTP_MAXDPOLL, 0, skeyid);
		if (peer == 0) {
			is_error = 1;
			break;
		}
		peer_config_manycast(peer2, peer);
		break;

	case AM_ERR:

		/*
		 * Something bad happened. Dirty floor will be mopped by
		 * the code at the end of this adventure.
		 */
		is_error = 1;
		break;

	case AM_NEWPASS:

		/*
		 * Okay, we're going to keep him around.  Allocate him
		 * some memory. But, don't do that unless the packet is
		 * properly authenticated.
		 */
		if ((sys_authenticate && !is_authentic)) {
			is_error = 1;
	    		break;
		}
		peer = newpeer(&rbufp->recv_srcadr, rbufp->dstadr,
		    MODE_PASSIVE, PKT_VERSION(pkt->li_vn_mode),
	 	    NTP_MINDPOLL, NTP_MAXDPOLL, 0, skeyid);
		break;

	case AM_NEWBCL:

		/*
		 * Broadcast client being set up now. Do this only if
		 * the packet is properly authenticated.
		 */
		if ((restrict_mask & RES_NOPEER) || !sys_bclient ||
		    (sys_authenticate && !is_authentic)) {
			is_error = 1;
			break;
		}
		peer = newpeer(&rbufp->recv_srcadr, rbufp->dstadr,
		    MODE_MCLIENT, PKT_VERSION(pkt->li_vn_mode),
		    NTP_MINDPOLL, NTP_MAXDPOLL, 0, skeyid);
		if (peer == 0)
			break;
		peer->flags |= FLAG_MCAST1 | FLAG_MCAST2 | FLAG_BURST;
		peer->hmode = MODE_CLIENT;
		break;

	case AM_POSSBCL:
	case AM_PROCPKT:

		/*
		 * It seems like it is okay to process the packet now
		 */
		break;

	default:

		/*
		 * shouldn't be getting here, but simply return anyway!
		 */
		is_error = 1;
	}
	if (is_error) {

		/*
		 * Error stub. If we get here, something broke. We
		 * scuttle the autokey if necessary and sink the ship.
		 * This can occur only upon mobilization, so we can
		 * throw the structure away without fear of breaking
		 * anything.
		 */
		if (!is_mystic && skeyid > NTP_MAXKEY)
			authtrust(skeyid, 0);
		if (peer != 0)
			if (!(peer->flags & FLAG_CONFIG))
				unpeer(peer);
#ifdef DEBUG
		if (debug)
			printf("match error code %d assoc %d\n",
			    retcode, peer_associations);
#endif
		return;
	}

	/*
	 * If the peer isn't configured, set his keyid and authenable
	 * status based on the packet.
	 */
	oflags = peer->flags;
	peer->timereceived = current_time;
	if (!(peer->flags & FLAG_CONFIG) && has_mac) {
		peer->flags |= FLAG_AUTHENABLE;
		if (skeyid > NTP_MAXKEY) {
			if (peer->flags & FLAG_MCAST2)
				peer->keyid = skeyid;
			else
				peer->flags |= FLAG_SKEY;
		}
	}

	/*
	 * Determine if this guy is basically trustable. If not, flush
	 * the bugger. If this is the first packet that is
	 * authenticated, flush the clock filter. This is to foil
	 * clogging attacks that might starve the poor dear.
	 */
	peer->flash = 0;
	if (is_authentic)
		peer->flags |= FLAG_AUTHENTIC;
	else
		peer->flags &= ~FLAG_AUTHENTIC;
	if (peer->hmode == MODE_BROADCAST && (restrict_mask &
	    RES_DONTTRUST))
		peer->flash |= TEST10;		/* access denied */
	if (peer->flags & FLAG_AUTHENABLE) {
		if (!(peer->flags & FLAG_AUTHENTIC))
			peer->flash |= TEST5;	/* auth failed */
		else if (skeyid == 0)
			peer->flash |= TEST9;	/* peer not auth */
		else if (!(oflags & FLAG_AUTHENABLE)) {
			peer_clear(peer);
			report_event(EVNT_PEERAUTH, peer);
		}
	}
	if ((peer->flash & ~(u_int)TEST9) != 0) {

		/*
		 * The packet is bogus, so we throw it away before
		 * becoming a denial-of-service hazard. We don't throw
		 * the current association away if it is configured or
		 * if it has prior reachable friends.
		 */
		if (!is_mystic && skeyid > NTP_MAXKEY)
			authtrust(skeyid, 0);
		if (!(peer->flags & FLAG_CONFIG) && peer->reach == 0)
			unpeer(peer);
#ifdef DEBUG
		if (debug)
			printf(
			    "invalid packet 0x%02x code %d assoc %d\n",
			    peer->flash, retcode, peer_associations);
#endif
		return;
	}

#ifdef MD5
	/*
	 * The autokey dance. The cha-cha requires that the hash of the
	 * current session key matches the previous key identifier.
	 * Heaps of trouble if the steps falter.
	 */
	if (skeyid > NTP_MAXKEY) {
		int i;

		/*
		 * In the case of a new autokey, verify the hash matches
		 * one of the previous four hashes. If not, raise the
		 * authentication flasher and hope the next one works.
		 */
		if (hismode == MODE_SERVER) {
			peer->pkeyid = peer->keyid;
		} else if (peer->flags & FLAG_MCAST2) {
			if (peer->pkeyid > NTP_MAXKEY)
				authtrust(peer->pkeyid, 0);
			for (i = 0; i < 4 && tkeyid != peer->pkeyid;
			    i++) {
				tkeyid = session_key(
					ntohl((&rbufp->recv_srcadr)->sin_addr.s_addr),
					ntohl(rbufp->dstadr->bcast.sin_addr.s_addr),
					tkeyid, 0);
			}
		} else {
			if (peer->pkeyid > NTP_MAXKEY)
				authtrust(peer->pkeyid, 0);
			for (i = 0; i < 4 && tkeyid != peer->pkeyid;
			    i++) {
				tkeyid = session_key(
				    ntohl((&rbufp->recv_srcadr)->sin_addr.s_addr),
				    ntohl(rbufp->dstadr->sin.sin_addr.s_addr),
				    tkeyid, 0);
			}
		}
#ifdef XXX /* temp until certificate code is mplemented */
		if (tkeyid != peer->pkeyid)
			peer->flash |= TEST9;	/* peer not authentic */
#endif
		peer->pkeyid = skeyid;
	}
#endif /* MD5 */

	/*
	 * Gawdz, it's come to this. Process the dang packet. If
	 * something breaks and the association doesn't deserve to live,
	 * toss it. Be careful in active mode and return a packet
	 * anyway.
	 */
	process_packet(peer, pkt, &(rbufp->recv_time));
	if (!(peer->flags & FLAG_CONFIG) && peer->reach == 0) {
		if (peer->hmode == MODE_PASSIVE) {
			if (is_authentic)
				fast_xmit(rbufp, MODE_PASSIVE, skeyid);
			else
				fast_xmit(rbufp, MODE_PASSIVE, 0);
		}
		unpeer(peer);
	}
}


/*
 * process_packet - Packet Procedure, a la Section 3.4.4 of the
 *	specification. Or almost, at least. If we're in here we have a
 *	reasonable expectation that we will be having a long term
 *	relationship with this host.
 */
int
process_packet(
	register struct peer *peer,
	register struct pkt *pkt,
	l_fp *recv_ts
	)
{
	l_fp t10, t23;
	double p_offset, p_del, p_disp;
	double dtemp;
	l_fp p_rec, p_xmt, p_org, p_reftime;
	l_fp ci;
	int pmode;

	/*
	 * Swap header fields and keep the books.
	 */
	sys_processed++;
	peer->processed++;
	p_del = FPTOD(NTOHS_FP(pkt->rootdelay));
	p_disp = FPTOD(NTOHS_FP(pkt->rootdispersion));
	NTOHL_FP(&pkt->reftime, &p_reftime);
	NTOHL_FP(&pkt->rec, &p_rec);
	NTOHL_FP(&pkt->xmt, &p_xmt);
	if (PKT_MODE(pkt->li_vn_mode) != MODE_BROADCAST)
		NTOHL_FP(&pkt->org, &p_org);
	else
		p_org = peer->rec;
	peer->rec = *recv_ts;
	peer->ppoll = pkt->ppoll;
	pmode = PKT_MODE(pkt->li_vn_mode);

	/*
	 * Test for old or duplicate packets (tests 1 through 3).
	 */
	if (L_ISHIS(&peer->org, &p_xmt))	/* count old packets */
		peer->oldpkt++;
	if (L_ISEQU(&peer->org, &p_xmt))	/* test 1 */
		peer->flash |= TEST1;		/* duplicate packet */
	if (PKT_MODE(pkt->li_vn_mode) != MODE_BROADCAST) {
		if (!L_ISEQU(&peer->xmt, &p_org)) { /* test 2 */
			peer->bogusorg++;
			peer->flash |= TEST2;	/* bogus packet */
		}
		if (L_ISZERO(&p_rec) || L_ISZERO(&p_org))
			peer->flash |= TEST3;	/* unsynchronized */
	} else {
		if (L_ISZERO(&p_org))
			peer->flash |= TEST3;	/* unsynchronized */
	}
	peer->org = p_xmt;

	/*
	 * Test for valid header (tests 5 through 10)
	 */
	ci = p_xmt;
	L_SUB(&ci, &p_reftime);
	LFPTOD(&ci, dtemp);
	if (PKT_LEAP(pkt->li_vn_mode) == LEAP_NOTINSYNC || /* test 6 */
	    PKT_TO_STRATUM(pkt->stratum) >= NTP_MAXSTRATUM ||
	    dtemp < 0)
		peer->flash |= TEST6;	/* peer clock unsynchronized */
	if (!(peer->flags & FLAG_CONFIG) && sys_peer != 0) { /* test 7 */
		if (PKT_TO_STRATUM(pkt->stratum) > sys_stratum) {
			peer->flash |= TEST7; /* peer stratum too high */
			sys_badstratum++;
		}
	}
	if (fabs(p_del) >= MAXDISPERSE	/* test 8 */
	    || p_disp >= MAXDISPERSE)
		peer->flash |= TEST8;	/* delay/dispersion too high */

	/*
	 * If the packet header is invalid (tests 5 through 10), exit.
	 * XXX we let TEST9 sneak by until the certificate code is
	 * implemented, but only to mobilize the association.
	 */
	if (peer->flash & (TEST5 | TEST6 | TEST7 | TEST8 | TEST10)) {
#ifdef DEBUG
		if (debug)
			printf(
			    "invalid packet header 0x%02x mode %d\n",
			    peer->flash, pmode);
#endif
		return (0);
	}

	/*
	 * Valid header; update our state.
	 */
	record_raw_stats(&peer->srcadr, &peer->dstadr->sin,
	    &p_org, &p_rec, &p_xmt, &peer->rec);

	peer->leap = PKT_LEAP(pkt->li_vn_mode);
	peer->pmode = pmode;		/* unspec */
	peer->stratum = PKT_TO_STRATUM(pkt->stratum);
	peer->precision = pkt->precision;
	peer->rootdelay = p_del;
	peer->rootdispersion = p_disp;
	peer->refid = pkt->refid;
	peer->reftime = p_reftime;
	if (peer->reach == 0) {
		report_event(EVNT_REACH, peer);
		peer->timereachable = current_time;
	}
	peer->reach |= 1;
	poll_update(peer, peer->hpoll);

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
	p_disp = CLOCK_PHI * (peer->rec.l_ui - p_org.l_ui);

	/*
	 * If running in a broadcast association, the clock offset is
	 * (t1 - t0) corrected by the one-way delay, but we can't
	 * measure that directly; therefore, we start up in
	 * client/server mode, calculate the clock offset, using the
	 * engineered refinement algorithms, while also receiving
	 * broadcasts. When a broadcast is received in client/server
	 * mode, we calculate a correction factor to use after switching
	 * back to broadcast mode. We know NTP_SKEWFACTOR == 16, which
	 * accounts for the simplified ei calculation.
	 *
	 * If FLAG_MCAST2 is set, we are a broadcast/multicast client.
	 * If FLAG_MCAST1 is set, we haven't calculated the propagation
	 * delay. If hmode is MODE_CLIENT, we haven't set the local
	 * clock in client/server mode. Initially, we come up
	 * MODE_CLIENT. When the clock is first updated and FLAG_MCAST2
	 * is set, we switch from MODE_CLIENT to MODE_BCLIENT.
	 */
	if (pmode == MODE_BROADCAST) {
		if (peer->flags & FLAG_MCAST1) {
			if (peer->hmode == MODE_BCLIENT)
				peer->flags &= ~FLAG_MCAST1;
			LFPTOD(&ci, p_offset);
			peer->estbdelay = peer->offset - p_offset;
			return (1);

		}
		DTOLFP(peer->estbdelay, &t10);
		L_ADD(&ci, &t10);
		p_del = peer->delay;
	} else {
		L_ADD(&ci, &t23);
		L_RSHIFT(&ci);
		L_SUB(&t23, &t10);
		LFPTOD(&t23, p_del);
	}
	LFPTOD(&ci, p_offset);
	if (fabs(p_del) >= MAXDISPERSE || p_disp >= MAXDISPERSE) /* test 4 */
		peer->flash |= TEST4;	/* delay/dispersion too big */

	/*
	 * If the packet data are invalid (tests 1 through 4), exit.
	 */
	if (peer->flash) {
#ifdef DEBUG
		if (debug)
			printf("invalid packet data 0x%02x mode %d\n",
			    peer->flash, pmode);
#endif
		return(1);
	}


	/*
	 * This one is valid. Mark it so, give it to clock_filter().
	 */
	clock_filter(peer, p_offset, p_del, fabs(p_disp));
	clock_select();
	record_peer_stats(&peer->srcadr, ctlpeerstatus(peer),
	    peer->offset, peer->delay, peer->disp,
	    SQRT(peer->variance));
	return(1);
}


/*
 * clock_update - Called at system process update intervals.
 */
static void
clock_update(void)
{
	u_char oleap;
	u_char ostratum;
	int i;
	struct peer *peer;

	/*
	 * Reset/adjust the system clock. Do this only if there is a
	 * system peer and we haven't seen that peer lately. Watch for
	 * timewarps here.
	 */
	if (sys_peer == 0)
		return;
	if (sys_peer->pollsw == FALSE || sys_peer->burst > 0)
		return;
	sys_peer->pollsw = FALSE;
#ifdef DEBUG
	if (debug)
		printf("clock_update: at %ld assoc %d \n", current_time,
		    peer_associations);
#endif
	oleap = sys_leap;
	ostratum = sys_stratum;
	switch (local_clock(sys_peer, sys_offset, sys_epsil)) {

		case -1:
		/*
		 * Clock is too screwed up. Just exit for now.
		 */
		report_event(EVNT_SYSFAULT, (struct peer *)0);
		exit(1);
		/*NOTREACHED*/

		case 1:
		/*
		 * Clock was stepped. Clear filter registers
		 * of all peers.
		 */
		for (i = 0; i < HASH_SIZE; i++) {
			for (peer = peer_hash[i]; peer != 0;
				peer =peer->next)
				peer_clear(peer);
		}
		NLOG(NLOG_SYNCSTATUS)
			msyslog(LOG_INFO, "synchronisation lost");
		sys_peer = 0;
		sys_stratum = STRATUM_UNSPEC;
		report_event(EVNT_CLOCKRESET, (struct peer *)0);
		break;

		default:
		/*
		 * Update the system stratum, leap bits, root delay,
		 * root dispersion, reference ID and reference time. We
		 * also update select dispersion and max frequency
		 * error.
		 */
		sys_stratum = sys_peer->stratum + 1;
		if (sys_stratum == 1)
			sys_refid = sys_peer->refid;
		else
			sys_refid = sys_peer->srcadr.sin_addr.s_addr;
		sys_reftime = sys_peer->rec;
		sys_rootdelay = sys_peer->rootdelay +
		    fabs(sys_peer->delay);
		sys_leap = leap_consensus;
	}
	if (oleap != sys_leap)
		report_event(EVNT_SYNCCHG, (struct peer *)0);
	if (ostratum != sys_stratum)
		report_event(EVNT_PEERSTCHG, (struct peer *)0);
}


/*
 * poll_update - update peer poll interval. See Section 3.4.9 of the
 *	   spec.
 */
void
poll_update(
	struct peer *peer,
	int hpoll
	)
{
	long update;

	/*
	 * The wiggle-the-poll-interval dance. Broadcasters dance only
	 * the minpoll beat. Reference clock partners sit this one out.
	 * Dancers surviving the clustering algorithm beat to the system
	 * clock. Broadcast clients are usually lead by their broadcast
	 * partner, but faster in the initial mating dance.
	 */
	if (peer->hmode == MODE_BROADCAST) {
		peer->hpoll = peer->minpoll;
	} else if (peer->flags & FLAG_SYSPEER) {
		peer->hpoll = sys_poll;
	} else {
		if (hpoll > peer->maxpoll)
			peer->hpoll = peer->maxpoll;
		else if (hpoll < peer->minpoll)
			peer->hpoll = peer->minpoll;
		else
			peer->hpoll = hpoll;
	}
	if (peer->burst > 0) {
		if (peer->nextdate != current_time)
			return;
		if (peer->flags & FLAG_REFCLOCK)
			peer->nextdate++;
		else if (peer->reach & 0x1)
			peer->nextdate += RANDPOLL(BURST_INTERVAL2);
		else
			peer->nextdate += RANDPOLL(BURST_INTERVAL1);
	} else {
		update = max(min(peer->ppoll, peer->hpoll),
		    peer->minpoll);
		peer->nextdate = peer->outdate + RANDPOLL(update);
	}
#ifdef DEBUG
	if (debug > 1)
		printf("poll_update: at %lu %s poll %d burst %d last %lu next %lu\n",
		    current_time, ntoa(&peer->srcadr), hpoll,
		    peer->burst, peer->outdate, peer->nextdate);
#endif
}


/*
 * clear - clear peer filter registers.  See Section 3.4.8 of the spec.
 */
void
peer_clear(
	register struct peer *peer
	)
{
	register int i;

	memset(CLEAR_TO_ZERO(peer), 0, LEN_CLEAR_TO_ZERO);
	peer->estbdelay = sys_bdelay;
	peer->hpoll = peer->minpoll;
	peer->pollsw = FALSE;
	peer->variance = MAXDISPERSE;
	peer->epoch = current_time;
	for (i = 0; i < NTP_SHIFT; i++) {
		peer->filter_order[i] = i;
		peer->filter_disp[i] = MAXDISPERSE;
		peer->filter_epoch[i] = current_time;
	}
	poll_update(peer, peer->minpoll);

	/*
	 * Since we have a chance to correct possible funniness in
	 * our selection of interfaces on a multihomed host, do so
	 * by setting us to no particular interface.
	 * WARNING: do so only in non-broadcast mode!
	 */
	if (peer->hmode != MODE_BROADCAST)
		peer->dstadr = any_interface;
}


/*
 * clock_filter - add incoming clock sample to filter register and run
 *		  the filter procedure to find the best sample.
 */
void
clock_filter(
	register struct peer *peer,
	double sample_offset,
	double sample_delay,
	double sample_disp
	)
{
	register int i, j, k, n = 0;
	register u_char *ord;
	double distance[NTP_SHIFT];
	double x, y, z, off;

	/*
	 * Update error bounds and calculate distances. Also initialize
	 * sort index vector.
	 */
	x = CLOCK_PHI * (current_time - peer->update);
	peer->update = current_time;
	ord = peer->filter_order;
	j = peer->filter_nextpt;
	for (i = 0; i < NTP_SHIFT; i++) {
		peer->filter_disp[j] += x;
		if (peer->filter_disp[j] > MAXDISPERSE)
			peer->filter_disp[j] = MAXDISPERSE;
		distance[i] = fabs(peer->filter_delay[j]) / 2 +
		    peer->filter_disp[j];
		ord[i] = j;
		if (--j < 0)
			j += NTP_SHIFT;
	}

	/*
	 * Insert the new sample at the beginning of the register.
	 */
	peer->filter_offset[peer->filter_nextpt] = sample_offset;
	peer->filter_delay[peer->filter_nextpt] = sample_delay;
	x = LOGTOD(peer->precision) + LOGTOD(sys_precision) +
	    sample_disp;
	peer->filter_disp[peer->filter_nextpt] = min(x, MAXDISPERSE);
	peer->filter_epoch[peer->filter_nextpt] = current_time;
	distance[0] = min(x + fabs(sample_delay) / 2, MAXDISTANCE);
	peer->filter_nextpt++;
	if (peer->filter_nextpt >= NTP_SHIFT)
		peer->filter_nextpt = 0;

	/*
	 * Sort the samples in the register by distance. The winning
	 * sample will be in ord[0]. Sort the samples only if they
	 * are younger than the Allen intercept.
	 */
	y = min(allan_xpt, NTP_SHIFT * ULOGTOD(sys_poll));
	for (n = 0; n < NTP_SHIFT && current_time -
	    peer->filter_epoch[ord[n]] <= y; n++) {
		for (j = 0; j < n; j++) {
			if (distance[j] > distance[n]) {
				x = distance[j];
				k = ord[j];
				distance[j] = distance[n];
				ord[j] = ord[n];
				distance[n] = x;
				ord[n] = k;
			}
		}
	} 
	
	/*
	 * Compute the error bound and standard error.
	 */
	x = y = z = off = 0.;
	for (i = NTP_SHIFT - 1; i >= 0; i--) {
		x = NTP_FWEIGHT * (x + peer->filter_disp[ord[i]]);
		if (i < n) {
			z += 1. / distance[i];
			off += peer->filter_offset[ord[i]] /
			    distance[i];
			y += DIFF(peer->filter_offset[ord[i]],
			    peer->filter_offset[ord[0]]);
		}
	}
	peer->delay = peer->filter_delay[ord[0]];
	peer->variance = min(y / n, MAXDISPERSE);
	peer->disp = min(x, MAXDISPERSE);
	peer->epoch = current_time;
	x = peer->offset;
	if (peer->flags & FLAG_BURST)
		peer->offset = off / z;
	else
		peer->offset = peer->filter_offset[ord[0]];

	/*
	 * A new sample is useful only if it is younger than the last
	 * one used.
	 */
	if (peer->filter_epoch[ord[0]] > peer->epoch) {
#ifdef DEBUG
		if (debug)
			printf("clock_filter: discard %lu\n",
			    peer->filter_epoch[ord[0]] - peer->epoch);
#endif
		return;
	}

	/*
	 * If the offset exceeds the dispersion by CLOCK_SGATE and the
	 * interval since the last update is less than twice the system
	 * poll interval, consider the update a popcorn spike and ignore
	 * it.
	 */
	if (fabs(x - peer->offset) > CLOCK_SGATE &&
	    peer->filter_epoch[ord[0]] - peer->epoch < (1 <<
	    (sys_poll + 1))) {
#ifdef DEBUG
		if (debug)
			printf("clock_filter: popcorn spike %.6f\n", x);
#endif
		return;
	}
	peer->epoch = peer->filter_epoch[ord[0]];
	peer->pollsw = TRUE;
#ifdef DEBUG
	if (debug)
		printf(
		    "clock_filter: offset %.6f delay %.6f disp %.6f std %.6f, age %lu\n",
		    peer->offset, peer->delay, peer->disp,
		    SQRT(peer->variance), current_time - peer->epoch);
#endif
}


/*
 * clock_select - find the pick-of-the-litter clock
 */
void
clock_select(void)
{
	register struct peer *peer;
	int i;
	int nlist, nl3;
	double d, e, f;
	int j;
	int n;
	int allow, found, k;
	double high, low;
	double synch[NTP_MAXCLOCK], error[NTP_MAXCLOCK];
	struct peer *osys_peer;
	struct peer *typeacts = 0;
	struct peer *typelocal = 0;
	struct peer *typepps = 0;
	struct peer *typeprefer = 0;
	struct peer *typesystem = 0;

	static int list_alloc = 0;
	static struct endpoint *endpoint = NULL;
	static int *index = NULL;
	static struct peer **peer_list = NULL;
	static u_int endpoint_size = 0;
	static u_int index_size = 0;
	static u_int peer_list_size = 0;

	/*
	 * Initialize. If a prefer peer does not survive this thing,
	 * the pps_update switch will remain zero.
	 */
	pps_update = 0;
	nlist = 0;
	low = 1e9;
	high = -1e9;
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
	 * peers we know about to find the peers which are most likely
	 * to succeed. We run through the list doing the sanity checks
	 * and trying to insert anyone who looks okay.
	 */
	nlist = nl3 = 0;	/* none yet */
	for (n = 0; n < HASH_SIZE; n++) {
		for (peer = peer_hash[n]; peer != 0; peer = peer->next) {
			peer->flags &= ~FLAG_SYSPEER;
			peer->status = CTL_PST_SEL_REJECT;
			if (peer->flags & FLAG_NOSELECT)
				continue;	/* noselect (survey) */
			if (peer->reach == 0)
				continue;	/* unreachable */
			if (peer->stratum > 1 && peer->refid ==
			    peer->dstadr->sin.sin_addr.s_addr)
				continue;	/* sync loop */
			if (root_distance(peer) >= MAXDISTANCE + 2 *
			    CLOCK_PHI * ULOGTOD(sys_poll)) {
				peer->seldisptoolarge++;
				continue;	/* noisy or broken */
			}

			/*
			 * Don't allow the local-clock or acts drivers
			 * in the kitchen at this point, unless the
			 * prefer peer. Do that later, but only if
			 * nobody else is around.
			 */
			if (peer->refclktype == REFCLK_LOCALCLOCK
#if defined(VMS) && defined(VMS_LOCALUNIT)
				/* wjm: local unit VMS_LOCALUNIT taken seriously */
				&& REFCLOCKUNIT(&peer->srcadr) != VMS_LOCALUNIT
#endif	/* VMS && VMS_LOCALUNIT */
				) {
				typelocal = peer;
				if (!(peer->flags & FLAG_PREFER))
					continue; /* no local clock */
			}
			if (peer->sstclktype == CTL_SST_TS_TELEPHONE) {
				typeacts = peer;
				if (!(peer->flags & FLAG_PREFER))
					continue; /* no acts */
			}

			/*
			 * If we get this far, we assume the peer is
			 * acceptable.
			 */
			peer->status = CTL_PST_SEL_SANE;
			peer_list[nlist++] = peer;

			/*
			 * Insert each interval endpoint on the sorted
			 * list.
			 */
			e = peer->offset;	 /* Upper end */
			f = root_distance(peer);
			e = e + f;
			for (i = nl3 - 1; i >= 0; i--) {
				if (e >= endpoint[index[i]].val)
					break;
				index[i + 3] = index[i];
			}
			index[i + 3] = nl3;
			endpoint[nl3].type = 1;
			endpoint[nl3++].val = e;

			e = e - f;		/* Center point */
			for ( ; i >= 0; i--) {
				if (e >= endpoint[index[i]].val)
					break;
				index[i + 2] = index[i];
			}
			index[i + 2] = nl3;
			endpoint[nl3].type = 0;
			endpoint[nl3++].val = e;

			e = e - f;		/* Lower end */
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
#ifdef DEBUG
	if (debug > 1)
		for (i = 0; i < nl3; i++)
		printf("select: endpoint %2d %.6f\n",
		   endpoint[index[i]].type, endpoint[index[i]].val);
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
			typeacts->status = CTL_PST_SEL_SANE;
			peer_list[0] = typeacts;
			nlist = 1;
		} else if (typelocal != 0) {
			typelocal->status = CTL_PST_SEL_SANE;
			peer_list[0] = typelocal;
			nlist = 1;
		} else {
			if (sys_peer != 0) {
				report_event(EVNT_PEERSTCHG,
				    (struct peer *)0);
				NLOG(NLOG_SYNCSTATUS)
				msyslog(LOG_INFO,
				    "synchronisation lost");
			}
			sys_peer = 0;
			return;
		}
	}
#ifdef DEBUG
	if (debug > 1)
		printf("select: low %.6f high %.6f\n", low, high);
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
		if (nlist > 1 && (low >= peer->offset ||
			peer->offset >= high))
			continue;
		peer->status = CTL_PST_SEL_CORRECT;
		d = root_distance(peer) + peer->stratum * MAXDISPERSE;
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
	if (debug > 1)
		for (i = 0; i < nlist; i++)
			printf("select: %s distance %.6f\n",
			    ntoa(&peer_list[i]->srcadr), synch[i]);
#endif

	/*
	 * Now, prune outlyers by root dispersion. Continue as long as
	 * there are more than NTP_MINCLOCK survivors and the minimum
	 * select dispersion is greater than the maximum peer
	 * dispersion. Stop if we are about to discard a prefer peer.
	 */
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		error[i] = peer->variance;
		if (i < NTP_CANCLOCK)
			peer->status = CTL_PST_SEL_SELCAND;
		else
			peer->status = CTL_PST_SEL_DISTSYSPEER;
	}
	while (1) {
		sys_maxd = 0;
		d = error[0];
		for (k = i = nlist - 1; i >= 0; i--) {
			double sdisp = 0;

			for (j = nlist - 1; j > 0; j--) {
				sdisp = NTP_SWEIGHT * (sdisp +
					DIFF(peer_list[i]->offset,
					peer_list[j]->offset));
			}
			if (sdisp > sys_maxd) {
				sys_maxd = sdisp;
				k = i;
			}
			if (error[i] < d)
				d = error[i];
		}

#ifdef DEBUG
		if (debug > 1)
			printf(
			    "select: survivors %d select %.6f peer %.6f\n",
			    nlist, SQRT(sys_maxd), SQRT(d));
#endif
		if (nlist <= NTP_MINCLOCK || sys_maxd <= d ||
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
			printf(
			    "select: %s offset %.6f, distance %.6f poll %d\n",
			    ntoa(&peer_list[i]->srcadr), peer_list[i]->offset,
			    synch[i], peer_list[i]->pollsw);
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
	 * peers. If a prefer peer is found within clock_max, update the
	 * pps switch. Of the other peers not at the lowest stratum,
	 * check if the system peer is among them and, if found, zap
	 * him. We note that the head of the list is at the lowest
	 * stratum and that unsynchronized peers cannot survive this
	 * far.
	 */
	leap_consensus = 0;
	for (i = nlist - 1; i >= 0; i--) {
		peer_list[i]->status = CTL_PST_SEL_SYNCCAND;
		peer_list[i]->flags |= FLAG_SYSPEER;
		poll_update(peer_list[i], peer_list[i]->hpoll);
		if (peer_list[i]->stratum == peer_list[0]->stratum) {
			leap_consensus |= peer_list[i]->leap;
			if (peer_list[i]->refclktype == REFCLK_ATOM_PPS)
				typepps = peer_list[i];
			if (peer_list[i] == sys_peer)
				typesystem = peer_list[i];
			if (peer_list[i]->flags & FLAG_PREFER) {
				typeprefer = peer_list[i];
				if (fabs(typeprefer->offset) <
				    clock_max)
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
	 * (type REFCLK_LOCALCLOCK with FLAG_PREFER) or prefer modem
	 * peers (type REFCLK_NIST_ATOM etc with FLAG_PREFER), (2) pps
	 * peers (type REFCLK_ATOM_PPS), (3) remaining prefer peers
	 * (flag FLAG_PREFER), (4) the existing system peer, if any, (5)
	 * the head of the survivor list. Note that only one peer can be
	 * declared prefer. The order of preference is in the order
	 * stated. Note that all of these must be at the lowest stratum,
	 * i.e., the stratum of the head of the survivor list.
	 */
	osys_peer = sys_peer;
	if (typeprefer && (typeprefer->refclktype == REFCLK_LOCALCLOCK
	    || typeprefer->sstclktype == CTL_SST_TS_TELEPHONE ||
		!typepps)) {
		sys_peer = typeprefer;
		sys_peer->status = CTL_PST_SEL_SYSPEER;
		sys_offset = sys_peer->offset;
		sys_epsil = sys_peer->variance;
#ifdef DEBUG
		if (debug > 1)
			printf("select: prefer offset %.6f\n",
			    sys_offset);
#endif
	} else if (typepps && pps_update) {
		sys_peer = typepps;
		sys_peer->status = CTL_PST_SEL_PPS;
		sys_offset = sys_peer->offset;
		sys_epsil = sys_peer->variance;
		if (!pps_control)
			NLOG(NLOG_SYSEVENT) /* conditional syslog */
				msyslog(LOG_INFO, "pps sync enabled");
		pps_control = current_time;
#ifdef DEBUG
		if (debug > 1)
			printf("select: pps offset %.6f\n", sys_offset);
#endif
	} else {
		if (!typesystem)
			sys_peer = peer_list[0];
		sys_peer->status = CTL_PST_SEL_SYSPEER;
		sys_offset = clock_combine(peer_list, nlist);
		sys_epsil = sys_peer->variance + sys_maxd;
#ifdef DEBUG
		if (debug > 1)
			printf("select: combine offset %.6f\n",
			   sys_offset);
#endif
	}
	if (osys_peer != sys_peer)
		report_event(EVNT_PEERSTCHG, (struct peer *)0);
	clock_update();
}

/*
 * clock_combine - combine offsets from selected peers
 */
static double
clock_combine(
	struct peer **peers,
	int npeers
	)
{
	int i;
	double x, y, z;
	y = z = 0;
	for (i = 0; i < npeers; i++) {
		x = root_distance(peers[i]);
		y += 1. / x;
		z += peers[i]->offset / x;
	}
	return (z / y);
}

/*
 * root_distance - compute synchronization distance from peer to root
 */
static double
root_distance(
	struct peer *peer
	)
{
	return ((fabs(peer->delay) + peer->rootdelay) / 2 +
		peer->rootdispersion + peer->disp +
		    SQRT(peer->variance) + CLOCK_PHI * (current_time -
		    peer->update));
}

/*
 * peer_xmit - send packet for persistent association.
 */
static void
peer_xmit(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct pkt xpkt;
	int find_rtt = (peer->cast_flags & MDF_MCAST) &&
		peer->hmode != MODE_BROADCAST;
	int sendlen;

	/*
	 * Initialize protocol fields.
	 */
	xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
		peer->version, peer->hmode);
	xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
	xpkt.ppoll = peer->hpoll;
	xpkt.precision = sys_precision;
	xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
	xpkt.rootdispersion = HTONS_FP(DTOUFP(sys_rootdispersion +
		LOGTOD(sys_precision)));
	xpkt.refid = sys_refid;
	HTONL_FP(&sys_reftime, &xpkt.reftime);
	HTONL_FP(&peer->org, &xpkt.org);
	HTONL_FP(&peer->rec, &xpkt.rec);

	/*
	 * Authenticate the packet if enabled and either configured or
	 * the previous packet was authenticated. If for some reason the
	 * key associated with the key identifier is not in the key
	 * cache, then honk key zero.
	 */
	sendlen = LEN_PKT_NOMAC;
	if (peer->flags & FLAG_AUTHENABLE) {
		u_long xkeyid;
		l_fp xmt_tx;

		/*
		 * Transmit encrypted packet compensated for the
		 * encryption delay.
		 */
#ifdef MD5
		if (peer->flags & FLAG_SKEY) {

			/*
			 * In autokey mode, allocate and initialize a
			 * key list if not already done. Then, use the
			 * list in inverse order, discarding keys once
			 * used. Keep the latest key around until the
			 * next one, so clients can use client/server
			 * packets to compute propagation delay. Note we
			 * have to wait until the receive side of the
			 * socket is bound and the server address
			 * confirmed.
			 */
			if (ntohl(peer->dstadr->sin.sin_addr.s_addr) ==
			    0 &&
				ntohl(peer->dstadr->bcast.sin_addr.s_addr) == 0)
				peer->keyid = 0;
			else {
				 if (peer->keylist == 0) {
					make_keylist(peer);
				} else {
					authtrust(peer->keylist[peer->keynumber], 0);
					if (peer->keynumber == 0)
						make_keylist(peer);
					else {
						peer->keynumber--;
						xkeyid = peer->keylist[peer->keynumber];
						if (!authistrusted(xkeyid))
							make_keylist(peer);
					}
				}
				peer->keyid = peer->keylist[peer->keynumber];
				xpkt.keyid1 = htonl(2 * sizeof(u_int32));
				xpkt.keyid2 = htonl(sys_private);
				sendlen += 2 * sizeof(u_int32);
			}
		}
#endif /* MD5 */
		xkeyid = peer->keyid;
		get_systime(&peer->xmt);
		L_ADD(&peer->xmt, &sys_authdelay);
		HTONL_FP(&peer->xmt, &xpkt.xmt);
		sendlen += authencrypt(xkeyid, (u_int32 *)&xpkt,
		    sendlen);
		get_systime(&xmt_tx);
		sendpkt(&peer->srcadr, find_rtt ? any_interface :
		    peer->dstadr, ((peer->cast_flags & MDF_MCAST) &&
		    !find_rtt) ? ((peer->cast_flags & MDF_ACAST) ? -7 :
		    peer->ttl) : -7, &xpkt, sendlen);

		/*
		 * Calculate the encryption delay. Keep the minimum over
		 * the latest two samples.
		 */
		L_SUB(&xmt_tx, &peer->xmt);
		L_ADD(&xmt_tx, &sys_authdelay);
		sys_authdly[1] = sys_authdly[0];
		sys_authdly[0] = xmt_tx.l_uf;
		if (sys_authdly[0] < sys_authdly[1])
			sys_authdelay.l_uf = sys_authdly[0];
		else
			sys_authdelay.l_uf = sys_authdly[1];
		peer->sent++;
#ifdef DEBUG
		if (debug)
			printf(
			    "transmit: at %ld to %s mode %d keyid %08lx index %d\n",
			    current_time, ntoa(&peer->srcadr),
			    peer->hmode, xkeyid, peer->keynumber);
#endif
	} else {
		/*
		 * Transmit non-authenticated packet.
		 */
		get_systime(&(peer->xmt));
		HTONL_FP(&peer->xmt, &xpkt.xmt);
		sendpkt(&(peer->srcadr), find_rtt ? any_interface :
		    peer->dstadr, ((peer->cast_flags & MDF_MCAST) &&
		    !find_rtt) ? ((peer->cast_flags & MDF_ACAST) ? -7 :
		    peer->ttl) : -8, &xpkt, sendlen);
		peer->sent++;
#ifdef DEBUG
		if (debug)
			printf("transmit: at %ld to %s mode %d\n",
				current_time, ntoa(&peer->srcadr),
				peer->hmode);
#endif
	}
}

/*
 * fast_xmit - Send packet for nonpersistent association.
 */
static void
fast_xmit(
	struct recvbuf *rbufp,	/* receive packet pointer */
	int xmode,		/* transmit mode */
	u_long xkeyid		/* transmit key ID */
	)
{
	struct pkt xpkt;
	struct pkt *rpkt;
	int sendlen;
	l_fp xmt_ts;

	/*
	 * Initialize transmit packet header fields in the receive
	 * buffer provided. We leave some fields intact as received.
	 */
	rpkt = &rbufp->recv_pkt;
	xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
		PKT_VERSION(rpkt->li_vn_mode), xmode);
	xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
	xpkt.ppoll = rpkt->ppoll;
	xpkt.precision = sys_precision;
	xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
	xpkt.rootdispersion = HTONS_FP(DTOUFP(sys_rootdispersion +
		LOGTOD(sys_precision)));
	xpkt.refid = sys_refid;
	HTONL_FP(&sys_reftime, &xpkt.reftime);
	xpkt.org = rpkt->xmt;
	HTONL_FP(&rbufp->recv_time, &xpkt.rec);
	sendlen = LEN_PKT_NOMAC;
	if (rbufp->recv_length > sendlen) {
		l_fp xmt_tx;

		/*
		 * Transmit encrypted packet compensated for the
		 * encryption delay.
		 */
		if (xkeyid > NTP_MAXKEY) {
			xpkt.keyid1 = htonl(2 * sizeof(u_int32));
			xpkt.keyid2 = htonl(sys_private);
			sendlen += 2 * sizeof(u_int32);
		}
		get_systime(&xmt_ts);
		L_ADD(&xmt_ts, &sys_authdelay);
		HTONL_FP(&xmt_ts, &xpkt.xmt);
		sendlen += authencrypt(xkeyid, (u_int32 *)&xpkt,
		    sendlen);
		get_systime(&xmt_tx);
		sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, -9, &xpkt,
		    sendlen);

		/*
		 * Calculate the encryption delay. Keep the minimum over
		 * the latest two samples.
		 */
		L_SUB(&xmt_tx, &xmt_ts);
		L_ADD(&xmt_tx, &sys_authdelay);
		sys_authdly[1] = sys_authdly[0];
		sys_authdly[0] = xmt_tx.l_uf;
		if (sys_authdly[0] < sys_authdly[1])
			sys_authdelay.l_uf = sys_authdly[0];
		else
			sys_authdelay.l_uf = sys_authdly[1];
#ifdef DEBUG
		if (debug)
			printf(
			    "transmit: at %ld to %s mode %d keyid %08lx\n",
				current_time, ntoa(&rbufp->recv_srcadr),
				xmode, xkeyid);
#endif
	} else {

		/*
		 * Transmit non-authenticated packet.
		 */
		get_systime(&xmt_ts);
		HTONL_FP(&xmt_ts, &xpkt.xmt);
		sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, -10, &xpkt,
			sendlen);
#ifdef DEBUG
		if (debug)
			printf("transmit: at %ld to %s mode %d\n",
				current_time, ntoa(&rbufp->recv_srcadr),
				xmode);
#endif
	}
}

#ifdef MD5
/*
 * Compute key list
 */
static void
make_keylist(
	struct peer *peer
	)
{
	int i;
	u_long keyid;
	u_long ltemp;

	/*
	 * Allocate the key list if necessary.
	 */
	if (peer->keylist == 0)
		peer->keylist = (u_long *)emalloc(sizeof(u_long) *
		    NTP_MAXSESSION);

	/*
	 * Generate an initial key ID which is unique and greater than
	 * NTP_MAXKEY.
	 */
	while (1) {
		keyid = (u_long)RANDOM & 0xffffffff;
		if (keyid <= NTP_MAXKEY)
			continue;
		if (authhavekey(keyid))
			continue;
		break;
	}

	/*
	 * Generate up to NTP_MAXSESSION session keys. Stop if the
	 * next one would not be unique or not a session key ID or if
	 * it would expire before the next poll.
	 */
	ltemp = sys_automax;
	for (i = 0; i < NTP_MAXSESSION; i++) {
		peer->keylist[i] = keyid;
		peer->keynumber = i;
		keyid = session_key(
		    ntohl(peer->dstadr->sin.sin_addr.s_addr),
		    (peer->hmode == MODE_BROADCAST || (peer->flags &
		    FLAG_MCAST2)) ?
		    ntohl(peer->dstadr->bcast.sin_addr.s_addr):
		    ntohl(peer->srcadr.sin_addr.s_addr), keyid, ltemp);
		ltemp -= 1 << peer->hpoll;
		if (auth_havekey(keyid) || keyid <= NTP_MAXKEY ||
		    ltemp <= (1 << (peer->hpoll + 1)))
			break;
	}
}
#endif /* MD5 */

/*
 * Find the precision of this particular machine
 */
#define DUSECS	1000000 /* us in a s */
#define HUSECS	(1 << 20) /* approx DUSECS for shifting etc */
#define MINSTEP 5	/* minimum clock increment (us) */
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
int
default_get_precision(void)
{
	struct timeval tp;
#if !defined(SYS_WINNT) && !defined(VMS) && !defined(_SEQUENT_)
	struct timezone tzp;
#elif defined(VMS) || defined(_SEQUENT_)
	struct timezone {
		int tz_minuteswest;
		int tz_dsttime;
	} tzp;
#endif /* defined(VMS) || defined(_SEQUENT_) */
	long last;
	int i;
	long diff;
	long val;
	long usec;
#ifdef HAVE_GETCLOCK
	struct timespec ts;
#endif
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	u_long freq;
	size_t j;

	/* Try to see if we can find the frequency of of the counter
	 * which drives our timekeeping
	 */
	j = sizeof freq;
	i = sysctlbyname("kern.timecounter.frequency", &freq, &j , 0,
	    0);
	if (i)
		i = sysctlbyname("machdep.tsc_freq", &freq, &j , 0, 0);
	if (i)
		i = sysctlbyname("machdep.i586_freq", &freq, &j , 0, 0);
	if (i)
		i = sysctlbyname("machdep.i8254_freq", &freq, &j , 0,
		    0);
	if (!i) {
		for (i = 1; freq ; i--)
			freq >>= 1;
		return (i);
	}
#endif
	usec = 0;
	val = MAXSTEP;
#ifdef HAVE_GETCLOCK
	(void) getclock(TIMEOFDAY, &ts);
	tp.tv_sec = ts.tv_sec;
	tp.tv_usec = ts.tv_nsec / 1000;
#else /*  not HAVE_GETCLOCK */
	GETTIMEOFDAY(&tp, &tzp);
#endif /* not HAVE_GETCLOCK */
	last = tp.tv_usec;
	for (i = 0; i < MINLOOPS && usec < HUSECS;) {
#ifdef HAVE_GETCLOCK
		(void) getclock(TIMEOFDAY, &ts);
		tp.tv_sec = ts.tv_sec;
		tp.tv_usec = ts.tv_nsec / 1000;
#else /*  not HAVE_GETCLOCK */
		GETTIMEOFDAY(&tp, &tzp);
#endif /* not HAVE_GETCLOCK */
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
	NLOG(NLOG_SYSINFO)
		msyslog(LOG_INFO, "precision = %ld usec", val);
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
init_proto(void)
{
	l_fp dummy;

	/*
	 * Fill in the sys_* stuff.  Default is don't listen to
	 * broadcasting, authenticate.
	 */
	sys_leap = LEAP_NOTINSYNC;
	sys_stratum = STRATUM_UNSPEC;
	sys_precision = (s_char)default_get_precision();
	sys_rootdelay = 0;
	sys_rootdispersion = 0;
	sys_refid = 0;
	L_CLR(&sys_reftime);
	sys_peer = 0;
	get_systime(&dummy);
	sys_bclient = 0;
	sys_bdelay = DEFBROADDELAY;
#if defined(DES) || defined(MD5)
	sys_authenticate = 1;
#else
	sys_authenticate = 0;
#endif
	L_CLR(&sys_authdelay);
	sys_authdly[0] = sys_authdly[1] = 0;
	sys_stattime = 0;
	sys_badstratum = 0;
	sys_oldversionpkt = 0;
	sys_newversionpkt = 0;
	sys_badlength = 0;
	sys_unknownversion = 0;
	sys_processed = 0;
	sys_badauth = 0;
	sys_manycastserver = 0;
	sys_automax = 1 << NTP_AUTOMAX;

	/*
	 * Default these to enable
	 */
	ntp_enable = 1;
#ifndef KERNEL_FLL_BUG
	kern_enable = 1;
#endif
	msyslog(LOG_DEBUG, "kern_enable is %d", kern_enable);
	stats_control = 1;

	/*
	 * Some system clocks should only be adjusted in 10ms increments.
	 */
#if defined RELIANTUNIX_CLOCK
	systime_10ms_ticks = 1;		  /* Reliant UNIX */
#elif defined SCO5_CLOCK
	if (sys_precision >= (s_char)-10) /* pre-SCO OpenServer 5.0.6 */
		systime_10ms_ticks = 1;
#endif
	if (systime_10ms_ticks)
		msyslog(LOG_INFO, "using 10ms tick adjustments");
}


/*
 * proto_config - configure the protocol module
 */
void
proto_config(
	int item,
	u_long value,
	double dvalue
	)
{
	/*
	 * Figure out what he wants to change, then do it
	 */
	switch (item) {
	case PROTO_KERNEL:

		/*
		 * Turn on/off kernel discipline
		 */
		kern_enable = (int)value;
		break;

	case PROTO_NTP:

		/*
		 * Turn on/off clock discipline
		 */
		ntp_enable = (int)value;
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
		io_multicast_add(value);
		break;

	case PROTO_MULTICAST_DEL:

		/*
		 * Delete multicast group address
		 */
		io_multicast_del(value);
		break;

	case PROTO_BROADDELAY:

		/*
		 * Set default broadcast delay
		 */
		sys_bdelay = dvalue;
		break;

	case PROTO_AUTHENTICATE:

		/*
		 * Specify the use of authenticated data
		 */
		sys_authenticate = (int)value;
		break;

	default:

		/*
		 * Log this error
		 */
		msyslog(LOG_ERR,
		    "proto_config: illegal item %d, value %ld",
			item, value);
		break;
	}
}


/*
 * proto_clr_stats - clear protocol stat counters
 */
void
proto_clr_stats(void)
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
