/*
 * ntp_proto.c - NTP version 4 protocol machinery
 *
 * ATTENTION: Get approval from Dave Mills on all changes to this file!
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"
#include "ntp_control.h"
#include "ntp_string.h"

#include <stdio.h>

#if defined(VMS) && defined(VMS_LOCALUNIT)	/*wjm*/
#include "ntp_refclock.h"
#endif

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/sysctl.h>
#endif

/*
 * This macro defines the authentication state. If x is 1 authentication
 * is required; othewise it is optional.
 */
#define	AUTH(x, y)	((x) ? (y) == AUTH_OK : (y) == AUTH_OK || \
			    (y) == AUTH_NONE)

/*
 * System variables are declared here. See Section 3.2 of the
 * specification.
 */
u_char	sys_leap;		/* system leap indicator */
u_char	sys_stratum;		/* stratum of system */
s_char	sys_precision;		/* local clock precision (log2 s) */
double	sys_rootdelay;		/* roundtrip delay to primary source */
double	sys_rootdispersion;	/* dispersion to primary source */
u_int32 sys_refid;		/* source/loop in network byte order */
static	double sys_offset;	/* current local clock offset */
l_fp	sys_reftime;		/* time we were last updated */
struct	peer *sys_peer;		/* our current peer */
struct	peer *sys_pps;		/* our PPS peer */
struct	peer *sys_prefer;	/* our cherished peer */
int	sys_kod;		/* kod credit */
int	sys_kod_rate = 2;	/* max kod packets per second */
#ifdef OPENSSL
u_long	sys_automax;		/* maximum session key lifetime */
#endif /* OPENSSL */

/*
 * Nonspecified system state variables.
 */
int	sys_bclient;		/* broadcast client enable */
double	sys_bdelay;		/* broadcast client default delay */
int	sys_calldelay;		/* modem callup delay (s) */
int	sys_authenticate;	/* requre authentication for config */
l_fp	sys_authdelay;		/* authentication delay */
static	u_long sys_authdly[2];	/* authentication delay shift reg */
static	double sys_mindisp = MINDISPERSE; /* min disp increment (s) */
static	double sys_maxdist = MAXDISTANCE; /* selection threshold (s) */
double	sys_jitter;		/* system jitter (s) */
static	int sys_hopper;		/* anticlockhop counter */
static	int sys_maxhop = MAXHOP; /* anticlockhop counter threshold */
int	leap_next;		/* leap consensus */
keyid_t	sys_private;		/* private value for session seed */
int	sys_manycastserver;	/* respond to manycast client pkts */
int	peer_ntpdate;		/* active peers in ntpdate mode */
int	sys_survivors;		/* truest of the truechimers */
#ifdef OPENSSL
char	*sys_hostname;		/* gethostname() name */
#endif /* OPENSSL */

/*
 * TOS and multicast mapping stuff
 */
int	sys_floor = 0;		/* cluster stratum floor */
int	sys_ceiling = STRATUM_UNSPEC; /* cluster stratum ceiling */
int	sys_minsane = 1;	/* minimum candidates */
int	sys_minclock = NTP_MINCLOCK; /* minimum survivors */
int	sys_maxclock = NTP_MAXCLOCK; /* maximum candidates */
int	sys_cohort = 0;		/* cohort switch */
int	sys_orphan = STRATUM_UNSPEC + 1; /* orphan stratum */
double	sys_orphandelay = 0;	/* orphan root delay */
int	sys_beacon = BEACON;	/* manycast beacon interval */
int	sys_ttlmax;		/* max ttl mapping vector index */
u_char	sys_ttl[MAX_TTL];	/* ttl mapping vector */

/*
 * Statistics counters
 */
u_long	sys_stattime;		/* time since reset */
u_long	sys_received;		/* packets received */
u_long	sys_processed;		/* packets processed */
u_long	sys_newversionpkt;	/* current version */
u_long	sys_oldversionpkt;	/* recent version */
u_long	sys_unknownversion;	/* invalid version */
u_long	sys_restricted;		/* access denied */
u_long	sys_badlength;		/* bad length or format */
u_long	sys_badauth;		/* bad authentication */
u_long	sys_limitrejected;	/* rate exceeded */

static	double	root_distance	P((struct peer *));
static	void	clock_combine	P((struct peer **, int));
static	void	peer_xmit	P((struct peer *));
static	void	fast_xmit	P((struct recvbuf *, int, keyid_t,
				    int));
static	void	clock_update	P((void));
static	int	default_get_precision	P((void));
static	int	peer_unfit	P((struct peer *));


/*
 * transmit - Transmit Procedure. See Section 3.4.2 of the
 *	specification.
 */
void
transmit(
	struct peer *peer	/* peer structure pointer */
	)
{
	int	hpoll;

	/*
	 * The polling state machine. There are two kinds of machines,
	 * those that never expect a reply (broadcast and manycast
	 * server modes) and those that do (all other modes). The dance
	 * is intricate...
	 */
	/*
	 * Orphan mode is active when enabled and when no servers less
	 * than the orphan statum are available. In this mode packets
	 * are sent at the orphan stratum. An orphan with no other
	 * synchronization source is an orphan parent. It assumes root
	 * delay zero and reference ID the loopback address. All others
	 * are orphan children with root delay randomized over a 1-s
	 * range. The root delay is used by the election algorithm to
	 * select the order of synchronization.
	 */
	hpoll = peer->hpoll;
	if (sys_orphan < STRATUM_UNSPEC && sys_peer == NULL) {
		sys_leap = LEAP_NOWARNING;
		sys_stratum = sys_orphan;
		sys_refid = htonl(LOOPBACKADR);
		sys_rootdelay = 0;
		sys_rootdispersion = 0;
	}

	/*
	 * In broadcast mode the poll interval is never changed from
	 * minpoll.
	 */
	if (peer->cast_flags & (MDF_BCAST | MDF_MCAST)) {
		peer->outdate = current_time;
		peer_xmit(peer);
		poll_update(peer, hpoll);
		return;
	}

	/*
	 * In manycast mode we start with unity ttl. The ttl is
	 * increased by one for each poll until either sys_maxclock
	 * servers have been found or the maximum ttl is reached. When
	 * sys_maxclock servers are found we stop polling until one or
	 * more servers have timed out or until less than minpoll
	 * associations turn up. In this case additional better servers
	 * are dragged in and preempt the existing ones.
	 */
	if (peer->cast_flags & MDF_ACAST) {
		peer->outdate = current_time;
		if (peer->unreach > sys_beacon) {
			peer->unreach = 0;
			peer->ttl = 0;
			peer_xmit(peer);
		} else if (sys_survivors < sys_minclock ||
		    peer_preempt < sys_maxclock) {
			if (peer->ttl < sys_ttlmax)
				peer->ttl++;
			peer_xmit(peer);
		}
		peer->unreach++;
		poll_update(peer, hpoll);
		return;
	}

	/*
	 * In unicast modes the dance is much more intricate. It is
	 * desigmed to back off whenever possible to minimize network
	 * traffic.
	 */
	if (peer->burst == 0) {
		u_char oreach;

		/*
		 * Update the reachability status. If not heard for
		 * three consecutive polls, stuff infinity in the clock
		 * filter. 
		 */
		oreach = peer->reach;
		peer->outdate = current_time;
		if (peer == sys_peer)
			sys_hopper++;
		peer->reach <<= 1;
		if (!(peer->reach & 0x07))
			clock_filter(peer, 0., 0., MAXDISPERSE);
		if (!peer->reach) {

			/*
			 * Here the peer is unreachable. If it was
			 * previously reachable, raise a trap.
			 */
			if (oreach) {
				report_event(EVNT_UNREACH, peer);
				peer->timereachable = current_time;
			}

			/*
			 * Send a burst if enabled, but only once after
			 * a peer becomes unreachable. If the prempt
			 * flag is dim, bump the unreach counter by one;
			 * otherwise, bump it by three.
			 */
			if (peer->flags & FLAG_IBURST &&
			    peer->unreach == 0) {
				peer->burst = NTP_BURST;
			}
			if (!(peer->flags & FLAG_PREEMPT))	
				peer->unreach++;
			else
				peer->unreach += 3;
		} else {

			/*
			 * Here the peer is reachable. Set the poll
			 * interval to the system poll interval. Send a
			 * burst only if enabled and the peer is fit.
			 *
			 * Respond to the peer evaluation produced by
			 * the selection algorithm. If less than the
			 * outlyer level, up the unreach by three. If
			 * there are excess associations, up the unreach
			 * by two if not a candidate and by one if so.
			 */
			if (!(peer->flags & FLAG_PREEMPT)) {
				peer->unreach = 0;
			} else if (peer->status < CTL_PST_SEL_SELCAND) {
				peer->unreach += 3;
			} else if (peer_preempt > sys_maxclock) {
				if (peer->status < CTL_PST_SEL_SYNCCAND)
					peer->unreach += 2;
				else
					peer->unreach++;
			} else {
				peer->unreach = 0;
			}
			hpoll = sys_poll;
			if (peer->flags & FLAG_BURST &&
			    !peer_unfit(peer))
				peer->burst = NTP_BURST;
		}

		/*
		 * Watch for timeout. If ephemeral or preemptable, toss
		 * the rascal; otherwise, bump the poll interval.
		 */ 
		if (peer->unreach >= NTP_UNREACH) {
			if (peer->flags & FLAG_PREEMPT ||
			    !(peer->flags & FLAG_CONFIG)) {
				peer_clear(peer, "TIME");
				unpeer(peer);
				return;
			} else {
				hpoll++;
			}
		}
	} else {
		peer->burst--;

		/*
		 * If a broadcast client at this point, the burst has
		 * concluded, so we switch to client mode and purge the
		 * keylist, since no further transmissions will be made.
		 */
		if (peer->burst == 0) {
			if (peer->cast_flags & MDF_BCLNT) {
				peer->hmode = MODE_BCLIENT;
#ifdef OPENSSL
				key_expire(peer);
#endif /* OPENSSL */
			}

			/*
			 * If ntpdate mode and the clock has not been
			 * set and all peers have completed the burst,
			 * we declare a successful failure.
			 */
			if (mode_ntpdate) {
				peer_ntpdate--;
				if (peer_ntpdate == 0) {
					msyslog(LOG_NOTICE,
					    "no reply; clock not set");
					exit (0);
				}
			}
		}
	}

	/*
	 * Do not transmit if in broadcast client mode. 
	 */
	if (peer->hmode != MODE_BCLIENT)
		peer_xmit(peer);
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
	register struct peer *peer;	/* peer structure pointer */
	register struct pkt *pkt;	/* receive packet pointer */
	int	hisversion;		/* packet version */
	int	hisleap;		/* packet leap indicator */
	int	hismode;		/* packet mode */
	int	hisstratum;		/* packet stratum */
	int	restrict_mask;		/* restrict bits */
	int	has_mac;		/* length of MAC field */
	int	authlen;		/* offset of MAC field */
	int	is_authentic = 0;	/* cryptosum ok */
	keyid_t	skeyid = 0;		/* key ID */
	struct sockaddr_storage *dstadr_sin; /* active runway */
	struct peer *peer2;		/* aux peer structure pointer */
	l_fp	p_org;			/* origin timestamp */
	l_fp	p_rec;			/* receive timestamp */
	l_fp	p_xmt;			/* transmit timestamp */
#ifdef OPENSSL
	keyid_t tkeyid = 0;		/* temporary key ID */
	keyid_t	pkeyid = 0;		/* previous key ID */
	struct autokey *ap;		/* autokey structure pointer */
	int	rval;			/* cookie snatcher */
#endif /* OPENSSL */
	int retcode = AM_NOMATCH;
	int	at_listhead;

	/*
	 * Monitor the packet and get restrictions. Note that the packet
	 * length for control and private mode packets must be checked
	 * by the service routines. Note that no statistics counters are
	 * recorded for restrict violations, since these counters are in
	 * the restriction routine. Note the careful distinctions here
	 * between a packet with a format error and a packet that is
	 * simply discarded without prejudice. Some restrictions have to
	 * be handled later in order to generate a kiss-of-death packet.
	 */
	/*
	 * Bogus port check is before anything, since it probably
	 * reveals a clogging attack.
	 */
	sys_received++;
	if (SRCPORT(&rbufp->recv_srcadr) == 0) {
		sys_badlength++;
		return;				/* bogus port */
	}
	at_listhead = ntp_monitor(rbufp);
	restrict_mask = restrictions(&rbufp->recv_srcadr, at_listhead);
#ifdef DEBUG
	if (debug > 1)
		printf("receive: at %ld %s<-%s flags %x restrict %03x\n",
		    current_time, stoa(&rbufp->dstadr->sin),
		    stoa(&rbufp->recv_srcadr),
		    rbufp->dstadr->flags, restrict_mask);
#endif
	if (restrict_mask & RES_IGNORE) {
		sys_restricted++;
		return;				/* ignore everything */
	}
	pkt = &rbufp->recv_pkt;
	hisversion = PKT_VERSION(pkt->li_vn_mode);
	hisleap = PKT_LEAP(pkt->li_vn_mode);
	hismode = (int)PKT_MODE(pkt->li_vn_mode);
	hisstratum = PKT_TO_STRATUM(pkt->stratum);
	if (hismode == MODE_PRIVATE) {
		if (restrict_mask & RES_NOQUERY) {
			sys_restricted++;
			return;			/* no query private */
		}
		process_private(rbufp, ((restrict_mask &
		    RES_NOMODIFY) == 0));
		return;
	}
	if (hismode == MODE_CONTROL) {
		if (restrict_mask & RES_NOQUERY) {
			sys_restricted++;
			return;			/* no query control */
		}
		process_control(rbufp, restrict_mask);
		return;
	}
	if (restrict_mask & RES_DONTSERVE) {
		sys_restricted++;
		return;				/* no time */
	}
	if (rbufp->recv_length < LEN_PKT_NOMAC) {
		sys_badlength++;
		return;				/* runt packet */
	}
	
	/*
	 * Version check must be after the query packets, since they
	 * intentionally use early version.
	 */
	if (hisversion == NTP_VERSION) {
		sys_newversionpkt++;		/* new version */
	} else if (!(restrict_mask & RES_VERSION) && hisversion >=
	    NTP_OLDVERSION) {
		sys_oldversionpkt++;		/* previous version */
	} else {
		sys_unknownversion++;
		return;				/* old version */
	}

	/*
	 * Figure out his mode and validate the packet. This has some
	 * legacy raunch that probably should be removed. In very early
	 * NTP versions mode 0 was equivalent to what later versions
	 * would interpret as client mode.
	 */
	if (hismode == MODE_UNSPEC) {
		if (hisversion == NTP_OLDVERSION) {
			hismode = MODE_CLIENT;
		} else {
			sys_badlength++;
			return;                 /* invalid mode */
		}
	}

	/*
	 * Parse the extension field if present. We figure out whether
	 * an extension field is present by measuring the MAC size. If
	 * the number of words following the packet header is 0, no MAC
	 * is present and the packet is not authenticated. If 1, the
	 * packet is a crypto-NAK; if 3, the packet is authenticated
	 * with DES; if 5, the packet is authenticated with MD5. If 2 or
	 * 4, the packet is a runt and discarded forthwith. If greater
	 * than 5, an extension field is present, so we subtract the
	 * length of the field and go around again.
	 */
	authlen = LEN_PKT_NOMAC;
	has_mac = rbufp->recv_length - authlen;
	while (has_mac > 0) {
		int temp;

		if (has_mac % 4 != 0 || has_mac < 0) {
			sys_badlength++;
			return;			/* bad MAC length */
		}
		if (has_mac == 1 * 4 || has_mac == 3 * 4 || has_mac ==
		    MAX_MAC_LEN) {
			skeyid = ntohl(((u_int32 *)pkt)[authlen / 4]);
			break;

		} else if (has_mac > MAX_MAC_LEN) {
			temp = ntohl(((u_int32 *)pkt)[authlen / 4]) &
			    0xffff;
			if (temp < 4 || temp > NTP_MAXEXTEN || temp % 4
			    != 0) {
				sys_badlength++;
				return;		/* bad MAC length */
			}
			authlen += temp;
			has_mac -= temp;
		} else {
			sys_badlength++;
			return;			/* bad MAC length */
		}
	}
#ifdef OPENSSL
	pkeyid = tkeyid = 0;
#endif /* OPENSSL */

	/*
	 * We have tossed out as many buggy packets as possible early in
	 * the game to reduce the exposure to a clogging attack. Now we
	 * have to burn some cycles to find the association and
	 * authenticate the packet if required. Note that we burn only
	 * MD5 cycles, again to reduce exposure. There may be no
	 * matching association and that's okay.
	 *
	 * More on the autokey mambo. Normally the local interface is
	 * found when the association was mobilized with respect to a
	 * designated remote address. We assume packets arriving from
	 * the remote address arrive via this interface and the local
	 * address used to construct the autokey is the unicast address
	 * of the interface. However, if the sender is a broadcaster,
	 * the interface broadcast address is used instead.
	 & Notwithstanding this technobabble, if the sender is a
	 * multicaster, the broadcast address is null, so we use the
	 * unicast address anyway. Don't ask.
	 */
	peer = findpeer(&rbufp->recv_srcadr, rbufp->dstadr,  hismode,
	    &retcode);
	dstadr_sin = &rbufp->dstadr->sin;
	NTOHL_FP(&pkt->org, &p_org);
	NTOHL_FP(&pkt->rec, &p_rec);
	NTOHL_FP(&pkt->xmt, &p_xmt);

	/*
	 * Authentication is conditioned by three switches:
	 *
	 * NOPEER  (RES_NOPEER) do not mobilize an association unless
	 *         authenticated
	 * NOTRUST (RES_DONTTRUST) do not allow access unless
	 *         authenticated (implies NOPEER)
	 * enable  (sys_authenticate) master NOPEER switch, by default
	 *         on
	 *
	 * The NOPEER and NOTRUST can be specified on a per-client basis
	 * using the restrict command. The enable switch if on implies
	 * NOPEER for all clients. There are four outcomes:
	 *
	 * NONE    The packet has no MAC.
	 * OK      the packet has a MAC and authentication succeeds
	 * ERROR   the packet has a MAC and authentication fails
	 * CRYPTO  crypto-NAK. The MAC has four octets only.
	 *
	 * Note: The AUTH(x, y) macro is used to filter outcomes. If x
	 * is zero, acceptable outcomes of y are NONE and OK. If x is
	 * one, the only acceptable outcome of y is OK.
	 */
	if (has_mac == 0) {
		is_authentic = AUTH_NONE; /* not required */
#ifdef DEBUG
		if (debug)
			printf("receive: at %ld %s<-%s mode %d code %d auth %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode, retcode,
			    is_authentic);
#endif
	} else if (has_mac == 4) {
			is_authentic = AUTH_CRYPTO; /* crypto-NAK */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d code %d keyid %08x len %d mac %d auth %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode, retcode,
			    skeyid, authlen, has_mac, is_authentic);
#endif
	} else {
#ifdef OPENSSL
		/*
		 * For autokey modes, generate the session key
		 * and install in the key cache. Use the socket
		 * broadcast or unicast address as appropriate.
		 */
		if (skeyid > NTP_MAXKEY) {
		
			/*
			 * More on the autokey dance (AKD). A cookie is
			 * constructed from public and private values.
			 * For broadcast packets, the cookie is public
			 * (zero). For packets that match no
			 * association, the cookie is hashed from the
			 * addresses and private value. For server
			 * packets, the cookie was previously obtained
			 * from the server. For symmetric modes, the
			 * cookie was previously constructed using an
			 * agreement protocol; however, should PKI be
			 * unavailable, we construct a fake agreement as
			 * the EXOR of the peer and host cookies.
			 *
			 * hismode	ephemeral	persistent
			 * =======================================
			 * active	0		cookie#
			 * passive	0%		cookie#
			 * client	sys cookie	0%
			 * server	0%		sys cookie
			 * broadcast	0		0
			 *
			 * # if unsync, 0
			 * % can't happen
			 */
			if (hismode == MODE_BROADCAST) {

				/*
				 * For broadcaster, use the interface
				 * broadcast address when available;
				 * otherwise, use the unicast address
				 * found when the association was
				 * mobilized. However, if this is from
				 * the wildcard interface, game over.
				 */
				if (crypto_flags && rbufp->dstadr ==
				    any_interface) {
					sys_restricted++;
					return;	     /* no wildcard */
				}
				pkeyid = 0;
				if (!SOCKNUL(&rbufp->dstadr->bcast))
					dstadr_sin =
					    &rbufp->dstadr->bcast;
			} else if (peer == NULL) {
				pkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin, 0,
				    sys_private, 0);
			} else {
				pkeyid = peer->pcookie;
			}

			/*
			 * The session key includes both the public
			 * values and cookie. In case of an extension
			 * field, the cookie used for authentication
			 * purposes is zero. Note the hash is saved for
			 * use later in the autokey mambo.
			 */
			if (authlen > LEN_PKT_NOMAC && pkeyid != 0) {
				session_key(&rbufp->recv_srcadr,
				    dstadr_sin, skeyid, 0, 2);
				tkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin,
				    skeyid, pkeyid, 0);
			} else {
				tkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin,
				    skeyid, pkeyid, 2);
			}

		}
#endif /* OPENSSL */

		/*
		 * Compute the cryptosum. Note a clogging attack may
		 * succeed in bloating the key cache. If an autokey,
		 * purge it immediately, since we won't be needing it
		 * again. If the packet is authentic, it can mobilize an
		 * association. Note that there is no key zero.
		 */
		if (!authdecrypt(skeyid, (u_int32 *)pkt, authlen,
		    has_mac)) {
			is_authentic = AUTH_ERROR;
			sys_badauth++;
		} else {
			is_authentic = AUTH_OK;
		}
#ifdef OPENSSL
		if (skeyid > NTP_MAXKEY)
			authtrust(skeyid, 0);
#endif /* OPENSSL */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d code %d keyid %08x len %d mac %d auth %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode, retcode,
			    skeyid, authlen, has_mac, is_authentic);
#endif
	}

	/*
	 * The association matching rules are implemented by a set of
	 * routines and an association table. A packet matching an
	 * association is processed by the peer process for that
	 * association. If there are no errors, an ephemeral association
	 * is mobilized: a broadcast packet mobilizes a broadcast client
	 * aassociation; a manycast server packet mobilizes a manycast
	 * client association; a symmetric active packet mobilizes a
	 * symmetric passive association.
	 */
	switch (retcode) {

	/*
	 * This is a client mode packet not matching any association. If
	 * an ordinary client, simply toss a server mode packet back
	 * over the fence. If a manycast client, we have to work a
	 * little harder.
	 */
	case AM_FXMIT:

		/*
		 * The vanilla case is when this is not a multicast
		 * interface. If authentication succeeds, return a
		 * server mode packet; if not and the key ID is nonzero,
		 * return a crypto-NAK.
		 */
		if (!(rbufp->dstadr->flags & INT_MCASTOPEN)) {
			if (AUTH(restrict_mask & RES_DONTTRUST,
			   is_authentic))
				fast_xmit(rbufp, MODE_SERVER, skeyid,
				    restrict_mask);
			else if (is_authentic == AUTH_ERROR)
				fast_xmit(rbufp, MODE_SERVER, 0,
				    restrict_mask);
			return;			/* hooray */
		}

		/*
		 * This must be manycast. Do not respond if not
		 * configured as a manycast server.
		 */
		if (!sys_manycastserver) {
			sys_restricted++;
			return;			/* not enabled */
		}

		/*
		 * Do not respond if unsynchronized or stratum is below
		 * the floor or at or above the ceiling.
		 */
		if (sys_leap == LEAP_NOTINSYNC || sys_stratum <
		    sys_floor || sys_stratum >= sys_ceiling)
			return;			/* bad stratum */

		/*
		 * Do not respond if our stratum is greater than the
		 * manycaster or it has already synchronized to us.
		 */
		if (sys_peer == NULL || hisstratum < sys_stratum ||
		    (sys_cohort && hisstratum == sys_stratum) ||
		    rbufp->dstadr->addr_refid == pkt->refid)
			return;			/* no help */

		/*
		 * Respond only if authentication succeeds. Don't do a
		 * crypto-NAK, as that would not be useful.
		 */
		if (AUTH(restrict_mask & RES_DONTTRUST, is_authentic))
			fast_xmit(rbufp, MODE_SERVER, skeyid,
			    restrict_mask);

		return;				/* hooray */

	/*
	 * This is a server mode packet returned in response to a client
	 * mode packet sent to a multicast group address. The origin
	 * timestamp is a good nonce to reliably associate the reply
	 * with what was sent. If there is no match, that's curious and
	 * could be an intruder attempting to clog, so we just ignore
	 * it.
	 *
	 * If the packet is authentic and the manycast association is
	 * found, we mobilize a client association and copy pertinent
	 * variables from the manycast association to the new client
	 * association. If not, just ignore the packet.
	 *
	 * There is an implosion hazard at the manycast client, since
	 * the manycast servers send the server packet immediately. If
	 * the guy is already here, don't fire up a duplicate.
	 */
	case AM_MANYCAST:
		if (!AUTH(sys_authenticate | (restrict_mask &
		    (RES_NOPEER | RES_DONTTRUST)), is_authentic))
			return;			/* bad auth */

		if ((peer2 = findmanycastpeer(rbufp)) == NULL) {
			sys_restricted++;
			return;			/* not enabled */
		}
		if ((peer = newpeer(&rbufp->recv_srcadr,
		    rbufp->dstadr, MODE_CLIENT,
		    hisversion, NTP_MINDPOLL, NTP_MAXDPOLL,
		    FLAG_IBURST | FLAG_PREEMPT, MDF_UCAST | MDF_ACLNT,
		    0, skeyid)) == NULL)
			return;			/* system error */

		/*
		 * We don't need these, but it warms the billboards.
		 */
		peer->ttl = peer2->ttl;
		break;

	/*
	 * This is the first packet received from a broadcast server. If
	 * the packet is authentic and we are enabled as broadcast
	 * client, mobilize a broadcast client association. We don't
	 * kiss any frogs here.
	 */
	case AM_NEWBCL:
		if (!AUTH(sys_authenticate | (restrict_mask &
		    (RES_NOPEER | RES_DONTTRUST)), is_authentic))
			return;			/* bad auth */

		/*
		 * Do not respond if unsynchronized or stratum is below
		 * the floor or at or above the ceiling.
		 */
		if (hisleap == LEAP_NOTINSYNC || hisstratum <
		    sys_floor || hisstratum >= sys_ceiling)
			return;			/* bad stratum */

		switch (sys_bclient) {

		/*
		 * If not enabled, just skedaddle.
		 */
		case 0:
			sys_restricted++;
			return;			/* not enabled */

		/*
		 * Execute the initial volley in order to calibrate the
		 * propagation delay and run the Autokey protocol, if
		 * enabled.
		 */
		case 1:
			if ((peer = newpeer(&rbufp->recv_srcadr,
			    rbufp->dstadr, MODE_CLIENT, hisversion,
			    NTP_MINDPOLL, NTP_MAXDPOLL, FLAG_MCAST |
			    FLAG_IBURST, MDF_BCLNT, 0, skeyid)) ==
			    NULL)
				return;		/* system error */
#ifdef OPENSSL
			if (skeyid > NTP_MAXKEY)
				crypto_recv(peer, rbufp);
#endif /* OPENSSL */
			return;			/* hooray */


		/*
		 * Do not execute the initial volley.
		 */
		case 2:
#ifdef OPENSSL
			/*
			 * If a two-way exchange is not possible,
			 * neither is Autokey.
			 */
			if (skeyid > NTP_MAXKEY) {
				msyslog(LOG_INFO,
				    "receive: autokey requires two-way communication");
				return;		/* no autokey */
			}
#endif /* OPENSSL */
			if ((peer = newpeer(&rbufp->recv_srcadr,
			    rbufp->dstadr, MODE_BCLIENT, hisversion,
			    NTP_MINDPOLL, NTP_MAXDPOLL, 0, MDF_BCLNT, 0,
			    skeyid)) == NULL)
				return;		/* system error */
		}
		break;

	/*
	 * This is the first packet received from a symmetric active
	 * peer. If the packet is authentic and the first he sent,
	 * mobilize a passive association. If not, kiss the frog.
	 */
	case AM_NEWPASS:

		/*
		 * If the inbound packet is correctly authenticated and
		 * enabled, a symmetric passive association is
		 * mobilized. If not but correctly authenticated, a
		 * symmetric active response is sent. If authentication
		 * fails, send a crypto-NAK packet. 
		 */
		if (!AUTH(restrict_mask & RES_DONTTRUST, is_authentic))
		    {
			if (is_authentic == AUTH_ERROR)
				fast_xmit(rbufp, MODE_ACTIVE, 0,
				    restrict_mask);
			return;			/* bad auth */
		}
		if (!AUTH(sys_authenticate | (restrict_mask &
		    RES_NOPEER), is_authentic)) {
			fast_xmit(rbufp, MODE_ACTIVE, skeyid,
			    restrict_mask);
			return;			/* hooray */
		}

		/*
		 * Do not respond if stratum is below the floor.
		 */
		if (hisstratum < sys_floor)
			return;			/* bad stratum */

		if ((peer = newpeer(&rbufp->recv_srcadr,
		    rbufp->dstadr, MODE_PASSIVE, hisversion,
		    NTP_MINDPOLL, NTP_MAXDPOLL, 0, MDF_UCAST, 0,
		    skeyid)) == NULL)
			return;			/* system error */
		break;

	/*
	 * Process regular packet. Nothing special.
	 */
	case AM_PROCPKT:
		break;

	/*
	 * A passive packet matches a passive association. This is
	 * usually the result of reconfiguring a client on the fly. As
	 * this association might be legitamate and this packet an
	 * attempt to deny service, just ignore it.
	 */
	case AM_ERR:
		return;

	/*
	 * For everything else there is the bit bucket.
	 */
	default:
		return;
	}
	peer->flash &= ~PKT_TEST_MASK;

	/*
	 * Next comes a rigorous schedule of timestamp checking. If the
	 * transmit timestamp is zero, the server is horribly broken.
	 */
	if (L_ISZERO(&p_xmt)) {
		return;				/* read rfc1305 */

	/*
	 * If the transmit timestamp duplicates a previous one, the
	 * packet is a replay. This prevents the bad guys from replaying
	 * the most recent packet, authenticated or not.
	 */
	} else if (L_ISEQU(&peer->org, &p_xmt)) {
		peer->flash |= TEST1;
		peer->oldpkt++;
		return;				/* duplicate packet */
	

	/*
	 * If this is a broadcast mode packet, skip further checking.
	 */
	} else if (hismode != MODE_BROADCAST) {
		if (L_ISZERO(&p_org))
			peer->flash |= TEST3;	/* protocol unsynch */
		else if (!L_ISEQU(&p_org, &peer->xmt))
			peer->flash |= TEST2;	/* bogus packet */
	}

	/*
	 * Update the origin and destination timestamps. If
	 * unsynchronized or bogus abandon ship. If the crypto machine
	 * breaks, light the crypto bit and plaint the log.
	 */
	peer->org = p_xmt;
	peer->rec = rbufp->recv_time;
	if (peer->flash & PKT_TEST_MASK) {
#ifdef OPENSSL
		if (crypto_flags && (peer->flags & FLAG_SKEY)) {
			rval = crypto_recv(peer, rbufp);
			if (rval != XEVNT_OK) {
				peer_clear(peer, "CRYP");
				peer->flash |= TEST9; /* crypto error */
			}
		}
#endif /* OPENSSL */
		return;				/* unsynch */
	}

	/*
	 * The timestamps are valid and the receive packet matches the
	 * last one sent. If the packet is a crypto-NAK, the server
	 * might have just changed keys. We reset the association
	 * and restart the protocol.
	 */
	if (is_authentic == AUTH_CRYPTO) {
		peer_clear(peer, "AUTH");
		return;				/* crypto-NAK */

	/* 
	 * If the association is authenticated, the key ID is nonzero
	 * and received packets must be authenticated. This is designed
	 * to avoid a bait-and-switch attack, which was possible in past
	 * versions. If symmetric modes, return a crypto-NAK. The peer
	 * should restart the protocol.
	 */
	} else if (!AUTH(peer->keyid || (restrict_mask & RES_DONTTRUST),
	    is_authentic)) {
		peer->flash |= TEST5;
		if (hismode == MODE_ACTIVE || hismode == MODE_PASSIVE)
			fast_xmit(rbufp, MODE_ACTIVE, 0, restrict_mask);
		return;				/* bad auth */
	}

	/*
	 * That was hard and I am sweaty, but the packet is squeaky
	 * clean. Get on with real work.
	 */
	peer->received++;
	peer->timereceived = current_time;
	if (is_authentic == AUTH_OK)
		peer->flags |= FLAG_AUTHENTIC;
	else
		peer->flags &= ~FLAG_AUTHENTIC;
#ifdef OPENSSL
	/*
	 * More autokey dance. The rules of the cha-cha are as follows:
	 *
	 * 1. If there is no key or the key is not auto, do nothing.
	 *
	 * 2. If this packet is in response to the one just previously
	 *    sent or from a broadcast server, do the extension fields.
	 *    Otherwise, assume bogosity and bail out.
	 *
	 * 3. If an extension field contains a verified signature, it is
	 *    self-authenticated and we sit the dance.
	 *
	 * 4. If this is a server reply, check only to see that the
	 *    transmitted key ID matches the received key ID.
	 *
	 * 5. Check to see that one or more hashes of the current key ID
	 *    matches the previous key ID or ultimate original key ID
	 *    obtained from the broadcaster or symmetric peer. If no
	 *    match, sit the dance and wait for timeout.
	 *
	 * In case of crypto error, fire the orchestra and stop dancing.
	 * This is considered a permanant error, so light the crypto bit
	 * to suppress further requests. If preemptable or ephemeral,
	 * scuttle the ship.
	 */
	if (crypto_flags && (peer->flags & FLAG_SKEY)) {
		peer->flash |= TEST8;
		rval = crypto_recv(peer, rbufp);
		if (rval != XEVNT_OK) {
			peer_clear(peer, "CRYP");
			peer->flash |= TEST9;	/* crypto error */
			if (peer->flags & FLAG_PREEMPT ||
			    !(peer->flags & FLAG_CONFIG))
				unpeer(peer);
			return;

		} else if (hismode == MODE_SERVER) {
			if (skeyid == peer->keyid)
				peer->flash &= ~TEST8;
		} else if (!(peer->flash & TEST8)) {
			peer->pkeyid = skeyid;
		} else if ((ap = (struct autokey *)peer->recval.ptr) !=
		    NULL) {
			int i;

			for (i = 0; ; i++) {
				if (tkeyid == peer->pkeyid ||
				    tkeyid == ap->key) {
					peer->flash &= ~TEST8;
					peer->pkeyid = skeyid;
					break;
				}
				if (i > ap->seq)
					break;
				tkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin,
				    tkeyid, pkeyid, 0);
			}
		}
		if (!(peer->crypto & CRYPTO_FLAG_PROV)) /* test 9 */
			peer->flash |= TEST8;	/* not proventic */

		/*
		 * If the transmit queue is nonempty, clamp the host
		 * poll interval to the packet poll interval.
		 */
		if (peer->cmmd != 0) {
			peer->ppoll = pkt->ppoll;
			poll_update(peer, peer->hpoll);
		}
	}
#endif /* OPENSSL */

	/*
	 * The dance is complete and the flash bits have been lit. Toss
	 * the packet over the fence for processing, which may light up
	 * more flashers.
	 */
	process_packet(peer, pkt);

	/*
	 * Well, that was nice. If TEST4 is lit, either the crypto
	 * machine jammed or a kiss-o'-death packet flew in, either of
	 * which is fatal.
	 */
	if (peer->flash & TEST4) {
		msyslog(LOG_INFO, "receive: fatal error %04x for %s",
		    peer->flash, stoa(&peer->srcadr));
		return;
	}
}


/*
 * process_packet - Packet Procedure, a la Section 3.4.4 of the
 *	specification. Or almost, at least. If we're in here we have a
 *	reasonable expectation that we will be having a long term
 *	relationship with this host.
 */
void
process_packet(
	register struct peer *peer,
	register struct pkt *pkt
	)
{
	double	t34, t21;
	double	p_offset, p_del, p_disp;
	l_fp	p_rec, p_xmt, p_org, p_reftime;
	l_fp	ci;
	u_char	pmode, pleap, pstratum;

	sys_processed++;
	peer->processed++;
	p_del = FPTOD(NTOHS_FP(pkt->rootdelay));
	p_disp = FPTOD(NTOHS_FP(pkt->rootdispersion));
	NTOHL_FP(&pkt->reftime, &p_reftime);
	NTOHL_FP(&pkt->rec, &p_rec);
	NTOHL_FP(&pkt->xmt, &p_xmt);
	pmode = PKT_MODE(pkt->li_vn_mode);
	pleap = PKT_LEAP(pkt->li_vn_mode);
	if (pmode != MODE_BROADCAST)
		NTOHL_FP(&pkt->org, &p_org);
	else
		p_org = peer->rec;
	pstratum = PKT_TO_STRATUM(pkt->stratum);

	/*
	 * Test for kiss-o'death packet)
	 */
	if (pleap == LEAP_NOTINSYNC && pstratum == STRATUM_UNSPEC) {
		if (memcmp(&pkt->refid, "DENY", 4) == 0) {
			peer_clear(peer, "DENY");
			peer->flash |= TEST4;	/* access denied */
		}
	}

	/*
	 * Capture the header values.
	 */
	record_raw_stats(&peer->srcadr, peer->dstadr ? &peer->dstadr->sin : NULL, &p_org,
	    &p_rec, &p_xmt, &peer->rec);
	peer->leap = pleap;
	peer->stratum = min(pstratum, STRATUM_UNSPEC);
	peer->pmode = pmode;
	peer->ppoll = pkt->ppoll;
	peer->precision = pkt->precision;
	peer->rootdelay = p_del;
	peer->rootdispersion = p_disp;
	peer->refid = pkt->refid;		/* network byte order */
	peer->reftime = p_reftime;

	/*
	 * Verify the server is synchronized; that is, the leap bits and
	 * stratum are valid, the root delay and root dispersion are
	 * valid and the reference timestamp is not later than the
	 * transmit timestamp.
	 */
	if (pleap == LEAP_NOTINSYNC ||		/* test 6 */
	    pstratum < sys_floor || pstratum >= sys_ceiling)
		peer->flash |= TEST6;		/* peer not synch */
	if (p_del < 0 || p_disp < 0 || p_del /	/* test 7 */
	    2 + p_disp >= MAXDISPERSE || !L_ISHIS(&p_xmt, &p_reftime))
		peer->flash |= TEST7;		/* bad header */

	/*
	 * If any tests fail at this point, the packet is discarded.
	 * Note that some flashers may have already been set in the
	 * receive() routine.
	 */
	if (peer->flash & PKT_TEST_MASK) {
#ifdef DEBUG
		if (debug)
			printf("packet: flash header %04x\n",
			    peer->flash);
#endif
		return;
	}
	if (!(peer->reach)) {
		report_event(EVNT_REACH, peer);
		peer->timereachable = current_time;
	}
	poll_update(peer, peer->hpoll);
	peer->reach |= 1;

	/*
	 * For a client/server association, calculate the clock offset,
	 * roundtrip delay and dispersion. The equations are reordered
	 * from the spec for more efficient use of temporaries. For a
	 * broadcast association, offset the last measurement by the
	 * computed delay during the client/server volley. Note that
	 * org has been set to the time of last reception. Note the
	 * computation of dispersion includes the system precision plus
	 * that due to the frequency error since the origin time.
	 *
	 * It is very important to respect the hazards of overflow. The
	 * only permitted operation on raw timestamps is subtraction,
	 * where the result is a signed quantity spanning from 68 years
	 * in the past to 68 years in the future. To avoid loss of
	 * precision, these calculations are done using 64-bit integer
	 * arithmetic. However, the offset and delay calculations are
	 * sums and differences of these first-order differences, which
	 * if done using 64-bit integer arithmetic, would be valid over
	 * only half that span. Since the typical first-order
	 * differences are usually very small, they are converted to 64-
	 * bit doubles and all remaining calculations done in floating-
	 * point arithmetic. This preserves the accuracy while retaining
	 * the 68-year span.
	 *
	 * Let t1 = p_org, t2 = p_rec, t3 = p_xmt, t4 = peer->rec:
	 */
	ci = p_xmt;			/* t3 - t4 */
	L_SUB(&ci, &peer->rec);
	LFPTOD(&ci, t34);
	ci = p_rec;			/* t2 - t1 */
	L_SUB(&ci, &p_org);
	LFPTOD(&ci, t21);
	ci = peer->rec;			/* t4 - t1 */
	L_SUB(&ci, &p_org);

	/*
	 * If running in a broadcast association, the clock offset is
	 * (t1 - t0) corrected by the one-way delay, but we can't
	 * measure that directly. Therefore, we start up in MODE_CLIENT
	 * mode, set FLAG_MCAST and exchange eight messages to determine
	 * the clock offset. When the last message is sent, we switch to
	 * MODE_BCLIENT mode. The next broadcast message after that
	 * computes the broadcast offset and clears FLAG_MCAST.
	 */
	if (pmode == MODE_BROADCAST) {
		p_offset = t34;
		if (peer->flags & FLAG_MCAST) {
			peer->estbdelay = peer->offset - p_offset;
			if (peer->hmode == MODE_CLIENT)
				return;

			peer->flags &= ~(FLAG_MCAST | FLAG_BURST);
		}
		p_offset += peer->estbdelay;
		p_del = peer->delay;
		p_disp = 0;
	} else {
		p_offset = (t21 + t34) / 2.;
		p_del = t21 - t34;
		LFPTOD(&ci, p_disp);
		p_disp = LOGTOD(sys_precision) +
		    LOGTOD(peer->precision) + clock_phi * p_disp;
	}
	p_del = max(p_del, LOGTOD(sys_precision));
	clock_filter(peer, p_offset, p_del, p_disp);
	record_peer_stats(&peer->srcadr, ctlpeerstatus(peer),
	    peer->offset, peer->delay, peer->disp, peer->jitter);
}


/*
 * clock_update - Called at system process update intervals.
 */
static void
clock_update(void)
{
	u_char	oleap;
	u_char	ostratum;
	double	dtemp;

	/*
	 * There must be a system peer at this point. If we just changed
	 * the system peer, but have a newer sample from the old one,
	 * wait until newer data are available.
	 */
	if (sys_poll < sys_peer->minpoll)
		sys_poll = sys_peer->minpoll;
	if (sys_poll > sys_peer->maxpoll)
		sys_poll = sys_peer->maxpoll;
	poll_update(sys_peer, sys_poll);
	if (sys_peer->epoch <= sys_clocktime)
		return;

#ifdef DEBUG
	if (debug)
		printf("clock_update: at %ld assoc %d \n", current_time,
		    peer_associations);
#endif
	oleap = sys_leap;
	ostratum = sys_stratum;
	switch (local_clock(sys_peer, sys_offset)) {

	/*
	 * Clock exceeds panic threshold. Life as we know it ends.
	 */
	case -1:
		report_event(EVNT_SYSFAULT, NULL);
		exit (-1);
		/* not reached */

	/*
	 * Clock was stepped. Flush all time values of all peers.
	 */
	case 2:
		clear_all();
		sys_leap = LEAP_NOTINSYNC;
		sys_stratum = STRATUM_UNSPEC;
		sys_peer = NULL;
		sys_rootdelay = 0;
		sys_rootdispersion = 0;
		memcpy(&sys_refid, "STEP", 4);
		report_event(EVNT_CLOCKRESET, NULL);
		break;

	/*
	 * Clock was slewed. Update the system stratum, leap bits, root
	 * delay, root dispersion, reference ID and reference time. If
	 * the leap changes, we gotta reroll the keys. Except for
	 * reference clocks, the minimum dispersion increment is not
	 * less than sys_mindisp.
	 */
	case 1:
		sys_leap = leap_next;
		sys_stratum = min(sys_peer->stratum + 1,
		    STRATUM_UNSPEC);
		sys_reftime = sys_peer->rec;

		/*
		 * In orphan mode the stratum defaults to the orphan
		 * stratum. The root delay is set to a random value
		 * generated at startup. The root dispersion is set from
		 * the peer dispersion; the peer root dispersion is
		 * ignored.
		 */
		dtemp = sys_peer->disp + clock_phi * (current_time -
		    sys_peer->update) + sys_jitter +
		    fabs(sys_peer->offset);
#ifdef REFCLOCK
		if (!(sys_peer->flags & FLAG_REFCLOCK) && dtemp <
		    sys_mindisp)
			dtemp = sys_mindisp;
#else
		if (dtemp < sys_mindisp)
			dtemp = sys_mindisp;
#endif /* REFCLOCK */
		if (sys_stratum >= sys_orphan) {
			sys_stratum = sys_orphan;
			sys_rootdelay = sys_peer->delay;
			sys_rootdispersion = dtemp;
		} else {
			sys_rootdelay = sys_peer->delay +
			    sys_peer->rootdelay;
			sys_rootdispersion = dtemp +
			    sys_peer->rootdispersion;
		}
		if (oleap == LEAP_NOTINSYNC) {
			report_event(EVNT_SYNCCHG, NULL);
#ifdef OPENSSL
			expire_all();
			crypto_update();
#endif /* OPENSSL */
		}
		break;
	/*
	 * Popcorn spike or step threshold exceeded. Pretend it never
	 * happened.
	 */
	default:
		break;
	}
	if (ostratum != sys_stratum)
		report_event(EVNT_PEERSTCHG, NULL);
}


/*
 * poll_update - update peer poll interval
 */
void
poll_update(
	struct peer *peer,
	int	mpoll
	)
{
	int	hpoll;

	/*
	 * This routine figures out when the next poll should be sent.
	 * That turns out to be wickedly complicated. The big problem is
	 * that sometimes the time for the next poll is in the past.
	 * Watch out for races here between the receive process and the
	 * poll process. The key assertion is that, if nextdate equals
	 * current_time, the call is from the poll process; otherwise,
	 * it is from the receive process.
	 *
	 * First, bracket the poll interval according to the type of
	 * association and options. If a fixed interval is configured,
	 * use minpoll. This primarily is for reference clocks, but
	 * works for any association.
	 */
	if (peer->flags & FLAG_FIXPOLL) {
		hpoll = peer->minpoll;

	/*
	 * The ordinary case; clamp the poll interval between minpoll
	 * and maxpoll.
	 */
	} else {
		hpoll = max(min(peer->maxpoll, mpoll), peer->minpoll);
	}
#ifdef OPENSSL
	/*
	 * Bit of crass arrogance at this point. If the poll interval
	 * has changed and we have a keylist, the lifetimes in the
	 * keylist are probably bogus. In this case purge the keylist
	 * and regenerate it later.
	 */
	if (hpoll != peer->hpoll)
		key_expire(peer);
#endif /* OPENSSL */
	peer->hpoll = hpoll;

	/*
	 * Now we figure out if there is an override. If during the
	 * crypto protocol and a message is pending, make it wait not
	 * more than two seconds.
	 */
#ifdef OPENSSL
	if (peer->cmmd != NULL && (sys_leap != LEAP_NOTINSYNC ||
	    peer->crypto)) {
		peer->nextdate = current_time + RESP_DELAY;

	/*
	 * If we get called from the receive routine while a burst is
	 * pending, just slink away. If from the poll routine and a
	 * reference clock or a pending crypto response, delay for one
	 * second. If this is the first sent in a burst, wait for the
	 * modem to come up. For others in the burst, delay two seconds.
	 */
	} else if (peer->burst > 0) {
#else /* OPENSSL */
	if (peer->burst > 0) {
#endif /* OPENSSL */
		if (peer->nextdate != current_time)
			return;
#ifdef REFCLOCK
		else if (peer->flags & FLAG_REFCLOCK)
			peer->nextdate += RESP_DELAY;
#endif /* REFCLOCK */
		else if (peer->flags & (FLAG_IBURST | FLAG_BURST) &&
		    peer->burst == NTP_BURST)
			peer->nextdate += sys_calldelay;
		else
			peer->nextdate += BURST_DELAY;
	/*
	 * The ordinary case; use the minimum of the host and peer
	 * intervals, but not less than minpoll. In other words,
	 * oversampling is okay but understampling is evil.
	 */
	} else {
		peer->nextdate = peer->outdate +
		    RANDPOLL(max(min(peer->ppoll, hpoll),
		    peer->minpoll));
	}

	/*
	 * If the time for the next poll has already happened, bring it
	 * up to the next second after this one. This way the only way
	 * to get nexdate == current time is from the poll routine.
	 */
	if (peer->nextdate <= current_time)
		peer->nextdate = current_time + 1;
#ifdef DEBUG
	if (debug > 1)
		printf("poll_update: at %lu %s flags %04x poll %d burst %d last %lu next %lu\n",
		    current_time, ntoa(&peer->srcadr), peer->flags,
		    peer->hpoll, peer->burst, peer->outdate,
		    peer->nextdate);
#endif
}

/*
 * peer_crypto_clear - discard crypto information
 */
void
peer_crypto_clear(
		  struct peer *peer
		  )
{
	/*
	 * If cryptographic credentials have been acquired, toss them to
	 * Valhalla. Note that autokeys are ephemeral, in that they are
	 * tossed immediately upon use. Therefore, the keylist can be
	 * purged anytime without needing to preserve random keys. Note
	 * that, if the peer is purged, the cryptographic variables are
	 * purged, too. This makes it much harder to sneak in some
	 * unauthenticated data in the clock filter.
	 */
	DPRINTF(1, ("peer_crypto_clear: at %ld next %ld assoc ID %d\n",
		    current_time, peer->nextdate, peer->associd));

#ifdef OPENSSL
	peer->assoc = 0;
	peer->crypto = 0;

	if (peer->pkey != NULL)
		EVP_PKEY_free(peer->pkey);
	peer->pkey = NULL;

	peer->digest = NULL;	/* XXX MEMLEAK? check whether this needs to be freed in any way - never was freed */

	if (peer->subject != NULL)
		free(peer->subject);
	peer->subject = NULL;

	if (peer->issuer != NULL)
		free(peer->issuer);
	peer->issuer = NULL;

	peer->pkeyid = 0;

	peer->pcookie = 0;

	if (peer->ident_pkey != NULL)
		EVP_PKEY_free(peer->ident_pkey);
	peer->ident_pkey = NULL;
	
	memset(&peer->fstamp, 0, sizeof(peer->fstamp));

	if (peer->iffval != NULL)
		BN_free(peer->iffval);
	peer->iffval = NULL;

	if (peer->grpkey != NULL)
		BN_free(peer->grpkey);
	peer->grpkey = NULL;

	value_free(&peer->cookval);
	value_free(&peer->recval);

	if (peer->cmmd != NULL) {
		free(peer->cmmd);
		peer->cmmd = NULL;
	}

	key_expire(peer);

	value_free(&peer->encrypt);
#endif /* OPENSSL */
}

/*
 * peer_clear - clear peer filter registers.  See Section 3.4.8 of the spec.
 */
void
peer_clear(
	struct peer *peer,		/* peer structure */
	char	*ident			/* tally lights */
	)
{
	int	i;

	peer_crypto_clear(peer);
	
	if (peer == sys_peer)
		sys_peer = NULL;

	/*
	 * Wipe the association clean and initialize the nonzero values.
	 */
	memset(CLEAR_TO_ZERO(peer), 0, LEN_CLEAR_TO_ZERO);
	peer->estbdelay = sys_bdelay;
	peer->ppoll = peer->maxpoll;
	peer->hpoll = peer->minpoll;
	peer->disp = MAXDISPERSE;
	peer->jitter = LOGTOD(sys_precision);
	for (i = 0; i < NTP_SHIFT; i++) {
		peer->filter_order[i] = i;
		peer->filter_disp[i] = MAXDISPERSE;
	}
#ifdef REFCLOCK
	if (!(peer->flags & FLAG_REFCLOCK)) {
		peer->leap = LEAP_NOTINSYNC;
		peer->stratum = STRATUM_UNSPEC;
		memcpy(&peer->refid, ident, 4);
	}
#else
	peer->leap = LEAP_NOTINSYNC;
	peer->stratum = STRATUM_UNSPEC;
	memcpy(&peer->refid, ident, 4);
#endif /* REFCLOCK */

	/*
	 * During initialization use the association count to spread out
	 * the polls at one-second intervals. Othersie, randomize over
	 * the minimum poll interval in order to avoid broadcast
	 * implosion.
	 */
	peer->nextdate = peer->update = peer->outdate = current_time;
	if (initializing)
		peer->nextdate += peer_associations;
	else if (peer->hmode == MODE_PASSIVE)
		peer->nextdate += RESP_DELAY;
	else
		peer->nextdate += (ntp_random() & ((1 << NTP_MINDPOLL) -
		    1));

	DPRINTF(1, ("peer_clear: at %ld next %ld assoc ID %d refid %s\n",
		    current_time, peer->nextdate, peer->associd, ident));
}


/*
 * clock_filter - add incoming clock sample to filter register and run
 *		  the filter procedure to find the best sample.
 */
void
clock_filter(
	struct peer *peer,		/* peer structure pointer */
	double	sample_offset,		/* clock offset */
	double	sample_delay,		/* roundtrip delay */
	double	sample_disp		/* dispersion */
	)
{
	double	dst[NTP_SHIFT];		/* distance vector */
	int	ord[NTP_SHIFT];		/* index vector */
	int	i, j, k, m;
	double	dtemp, etemp;

	/*
	 * Shift the new sample into the register and discard the oldest
	 * one. The new offset and delay come directly from the
	 * timestamp calculations. The dispersion grows from the last
	 * outbound packet or reference clock update to the present time
	 * and increased by the sum of the peer precision and the system
	 * precision. The delay can sometimes swing negative due to
	 * frequency skew, so it is clamped non-negative.
	 */
	j = peer->filter_nextpt;
	peer->filter_offset[j] = sample_offset;
	peer->filter_delay[j] = max(0, sample_delay);
	peer->filter_disp[j] = sample_disp;
	peer->filter_epoch[j] = current_time;
	j = (j + 1) % NTP_SHIFT;
	peer->filter_nextpt = j;

	/*
	 * Update dispersions since the last update and at the same
	 * time initialize the distance and index lists. The distance
	 * list uses a compound metric. If the sample is valid and
	 * younger than the minimum Allan intercept, use delay;
	 * otherwise, use biased dispersion.
	 */
	dtemp = clock_phi * (current_time - peer->update);
	peer->update = current_time;
	for (i = NTP_SHIFT - 1; i >= 0; i--) {
		if (i != 0)
			peer->filter_disp[j] += dtemp;
		if (peer->filter_disp[j] >= MAXDISPERSE) 
			peer->filter_disp[j] = MAXDISPERSE;
		if (peer->filter_disp[j] >= MAXDISPERSE)
			dst[i] = MAXDISPERSE;
		else if (peer->update - peer->filter_epoch[j] >
		    allan_xpt)
			dst[i] = sys_maxdist + peer->filter_disp[j];
		else
			dst[i] = peer->filter_delay[j];
		ord[i] = j;
		j++; j %= NTP_SHIFT;
	}

        /*
	 * If the clock discipline has stabilized, sort the samples in
	 * both lists by distance. Note, we do not displace a higher
	 * distance sample by a lower distance one unless lower by at
	 * least the precision.  
	 */
	if (state == 4) {
		for (i = 1; i < NTP_SHIFT; i++) {
			for (j = 0; j < i; j++) {
				if (dst[j] > dst[i] +
				    LOGTOD(sys_precision)) {
					k = ord[j];
					ord[j] = ord[i];
					ord[i] = k;
					etemp = dst[j];
					dst[j] = dst[i];
					dst[i] = etemp;
				}
			}
		}
	}

	/*
	 * Copy the index list to the association structure so ntpq
	 * can see it later. Prune the distance list to samples less
	 * than max distance, but keep at least two valid samples for
	 * jitter calculation.
	 */
	m = 0;
	for (i = 0; i < NTP_SHIFT; i++) {
		peer->filter_order[i] = (u_char) ord[i];
		if (dst[i] >= MAXDISPERSE || (m >= 2 && dst[i] >=
		    sys_maxdist))
			continue;
		m++;
	}
	
	/*
	 * Compute the dispersion and jitter. The dispersion is weighted
	 * exponentially by NTP_FWEIGHT (0.5) so it is normalized close
	 * to 1.0. The jitter is the RMS differences relative to the
	 * lowest delay sample. If no acceptable samples remain in the
	 * shift register, quietly tiptoe home leaving only the
	 * dispersion.
	 */
	peer->disp = peer->jitter = 0;
	k = ord[0];
	for (i = NTP_SHIFT - 1; i >= 0; i--) {
		j = ord[i];
		peer->disp = NTP_FWEIGHT * (peer->disp +
		    peer->filter_disp[j]);
		if (i < m)
			peer->jitter += DIFF(peer->filter_offset[j],
			    peer->filter_offset[k]);
	}

	/*
	 * If no acceptable samples remain in the shift register,
	 * quietly tiptoe home leaving only the dispersion. Otherwise,
	 * save the offset, delay and jitter. Note the jitter must not
	 * be less than the precision.
	 */
	if (m == 0)
		return;

	etemp = fabs(peer->offset - peer->filter_offset[k]);
	peer->offset = peer->filter_offset[k];
	peer->delay = peer->filter_delay[k];
	if (m > 1)
		peer->jitter /= m - 1;
	peer->jitter = max(SQRT(peer->jitter), LOGTOD(sys_precision));

	/*
	 * A new sample is useful only if it is younger than the last
	 * one used. Note the order is FIFO if the clock discipline has
	 * not stabilized.
	 */
	if (peer->filter_epoch[k] <= peer->epoch) {
#ifdef DEBUG
		if (debug)
			printf("clock_filter: discard %lu\n",
			    peer->epoch - peer->filter_epoch[k]);
#endif
		return;
	}

	/*
	 * If the difference between the last offset and the current one
	 * exceeds the jitter by CLOCK_SGATE and the interval since the
	 * last update is less than twice the system poll interval,
	 * consider the update a popcorn spike and ignore it.
	 */
	if (etemp > CLOCK_SGATE * peer->jitter && m > 1 &&
	    peer->filter_epoch[k] - peer->epoch < 2. *
	    ULOGTOD(sys_poll)) {
#ifdef DEBUG
		if (debug)
			printf("clock_filter: popcorn %.6f %.6f\n",
			    etemp, dtemp);
#endif
		return;
	}

	/*
	 * The mitigated sample statistics are saved for later
	 * processing. If not in a burst, tickle the select.
	 */
	peer->epoch = peer->filter_epoch[k];
#ifdef DEBUG
	if (debug)
		printf(
		    "clock_filter: n %d off %.6f del %.6f dsp %.6f jit %.6f, age %lu\n",
		    m, peer->offset, peer->delay, peer->disp,
		    peer->jitter, current_time - peer->epoch);
#endif
	if (peer->burst == 0 || sys_leap == LEAP_NOTINSYNC)
		clock_select();
}


/*
 * clock_select - find the pick-of-the-litter clock
 *
 * LOCKCLOCK: If the local clock is the prefer peer, it will always be
 * enabled, even if declared falseticker, (2) only the prefer peer can
 * be selected as the system peer, (3) if the external source is down,
 * the system leap bits are set to 11 and the stratum set to infinity.
 */
void
clock_select(void)
{
	struct peer *peer;
	int	i, j, k, n;
	int	nlist, nl3;

	int	allow, osurv;
	double	d, e, f, g;
	double	high, low;
	double	synch[NTP_MAXASSOC], error[NTP_MAXASSOC];
	struct peer *osys_peer;
	struct peer *typeacts = NULL;
	struct peer *typelocal = NULL;
	struct peer *typesystem = NULL;

	static int list_alloc = 0;
	static struct endpoint *endpoint = NULL;
	static int *indx = NULL;
	static struct peer **peer_list = NULL;
	static u_int endpoint_size = 0;
	static u_int indx_size = 0;
	static u_int peer_list_size = 0;

	/*
	 * Initialize and create endpoint, index and peer lists big
	 * enough to handle all associations.
	 */
	osys_peer = sys_peer;
	sys_peer = NULL;
	sys_pps = NULL;
	sys_prefer = NULL;
	osurv = sys_survivors;
	sys_survivors = 0;
#ifdef LOCKCLOCK
	sys_leap = LEAP_NOTINSYNC;
	sys_stratum = STRATUM_UNSPEC;
	memcpy(&sys_refid, "DOWN", 4);
#endif /* LOCKCLOCK */
	nlist = 0;
	for (n = 0; n < NTP_HASH_SIZE; n++)
		nlist += peer_hash_count[n];
	if (nlist > list_alloc) {
		if (list_alloc > 0) {
			free(endpoint);
			free(indx);
			free(peer_list);
		}
		while (list_alloc < nlist) {
			list_alloc += 5;
			endpoint_size += 5 * 3 * sizeof(*endpoint);
			indx_size += 5 * 3 * sizeof(*indx);
			peer_list_size += 5 * sizeof(*peer_list);
		}
		endpoint = (struct endpoint *)emalloc(endpoint_size);
		indx = (int *)emalloc(indx_size);
		peer_list = (struct peer **)emalloc(peer_list_size);
	}

	/*
	 * Initially, we populate the island with all the rifraff peers
	 * that happen to be lying around. Those with seriously
	 * defective clocks are immediately booted off the island. Then,
	 * the falsetickers are culled and put to sea. The truechimers
	 * remaining are subject to repeated rounds where the most
	 * unpopular at each round is kicked off. When the population
	 * has dwindled to sys_minclock, the survivors split a million
	 * bucks and collectively crank the chimes.
	 */
	nlist = nl3 = 0;	/* none yet */
	for (n = 0; n < NTP_HASH_SIZE; n++) {
		for (peer = peer_hash[n]; peer != NULL; peer =
		    peer->next) {
			peer->flags &= ~FLAG_SYSPEER;
			peer->status = CTL_PST_SEL_REJECT;

			/*
			 * Leave the island immediately if the peer is
			 * unfit to synchronize.
			 */
			if (peer_unfit(peer))
				continue;

			/*
			 * Don't allow the local clock or modem drivers
			 * in the kitchen at this point, unless the
			 * prefer peer. Do that later, but only if
			 * nobody else is around. These guys are all
			 * configured, so we never throw them away.
			 */
#ifdef REFCLOCK
			if (peer->refclktype == REFCLK_LOCALCLOCK
#if defined(VMS) && defined(VMS_LOCALUNIT)
			/* wjm: VMS_LOCALUNIT taken seriously */
			    && REFCLOCKUNIT(&peer->srcadr) !=
			    VMS_LOCALUNIT
#endif	/* VMS && VMS_LOCALUNIT */
				) {
				typelocal = peer;
#ifndef LOCKCLOCK
				if (!(peer->flags & FLAG_PREFER))
					continue; /* no local clock */
#endif /* LOCKCLOCK */
			}
			if (peer->sstclktype == CTL_SST_TS_TELEPHONE) {
				typeacts = peer;
				if (!(peer->flags & FLAG_PREFER))
					continue; /* no acts */
			}
#endif /* REFCLOCK */

			/*
			 * If we get this far, the peer can stay on the
			 * island, but does not yet have the immunity
			 * idol.
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
				if (e >= endpoint[indx[i]].val)
					break;

				indx[i + 3] = indx[i];
			}
			indx[i + 3] = nl3;
			endpoint[nl3].type = 1;
			endpoint[nl3++].val = e;

			e = e - f;		/* Center point */
			for (; i >= 0; i--) {
				if (e >= endpoint[indx[i]].val)
					break;

				indx[i + 2] = indx[i];
			}
			indx[i + 2] = nl3;
			endpoint[nl3].type = 0;
			endpoint[nl3++].val = e;

			e = e - f;		/* Lower end */
			for (; i >= 0; i--) {
				if (e >= endpoint[indx[i]].val)
					break;

				indx[i + 1] = indx[i];
			}
			indx[i + 1] = nl3;
			endpoint[nl3].type = -1;
			endpoint[nl3++].val = e;
		}
	}
#ifdef DEBUG
	if (debug > 2)
		for (i = 0; i < nl3; i++)
			printf("select: endpoint %2d %.6f\n",
			   endpoint[indx[i]].type,
			   endpoint[indx[i]].val);
#endif
	/*
	 * This is the actual algorithm that cleaves the truechimers
	 * from the falsetickers. The original algorithm was described
	 * in Keith Marzullo's dissertation, but has been modified for
	 * better accuracy.
	 *
	 * Briefly put, we first assume there are no falsetickers, then
	 * scan the candidate list first from the low end upwards and
	 * then from the high end downwards. The scans stop when the
	 * number of intersections equals the number of candidates less
	 * the number of falsetickers. If this doesn't happen for a
	 * given number of falsetickers, we bump the number of
	 * falsetickers and try again. If the number of falsetickers
	 * becomes equal to or greater than half the number of
	 * candidates, the Albanians have won the Byzantine wars and
	 * correct synchronization is not possible.
	 *
	 * Here, nlist is the number of candidates and allow is the
	 * number of falsetickers. Upon exit, the truechimers are the
	 * susvivors with offsets not less than low and not greater than
	 * high. There may be none of them.
	 */
	low = 1e9;
	high = -1e9;
	for (allow = 0; 2 * allow < nlist; allow++) {
		int	found;

		/*
		 * Bound the interval (low, high) as the largest
		 * interval containing points from presumed truechimers.
		 */
		found = 0;
		n = 0;
		for (i = 0; i < nl3; i++) {
			low = endpoint[indx[i]].val;
			n -= endpoint[indx[i]].type;
			if (n >= nlist - allow)
				break;
			if (endpoint[indx[i]].type == 0)
				found++;
		}
		n = 0;
		for (j = nl3 - 1; j >= 0; j--) {
			high = endpoint[indx[j]].val;
			n += endpoint[indx[j]].type;
			if (n >= nlist - allow)
				break;
			if (endpoint[indx[j]].type == 0)
				found++;
		}

		/*
		 * If the number of candidates found outside the
		 * interval is greater than the number of falsetickers,
		 * then at least one truechimer is outside the interval,
		 * so go around again. This is what makes this algorithm
		 * different than Marzullo's.
		 */
		if (found > allow)
			continue;

		/*
		 * If an interval containing truechimers is found, stop.
		 * If not, increase the number of falsetickers and go
		 * around again.
		 */
		if (high > low)
			break;
	}

	/*
	 * Clustering algorithm. Construct candidate list in order first
	 * by stratum then by root distance, but keep only the best
	 * NTP_MAXASSOC of them. Scan the list to find falsetickers, who
	 * leave the island immediately. The TRUE peer is always a
	 * truechimer. We must leave at least one peer to collect the
	 * million bucks. If in orphan mode, rascals found with lower
	 * stratum are guaranteed a seat on the bus.
	 */
	j = 0;
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		if (nlist > 1 && (peer->offset <= low || peer->offset >=
		    high) && !(peer->flags & FLAG_TRUE) &&
		    !(sys_stratum >= sys_orphan && peer->stratum <
		    sys_orphan))
			continue;

		peer->status = CTL_PST_SEL_DISTSYSPEER;

		/*
		 * The order metric is formed from the stratum times
		 * max distance (1.) plus the root distance. It strongly
		 * favors the lowest stratum, but a higher stratum peer
		 * can capture the clock if the low stratum dominant
		 * hasn't been heard for awhile.
		 */
		d = root_distance(peer) + peer->stratum * sys_maxdist;
		if (j >= NTP_MAXASSOC) {
			if (d >= synch[j - 1])
				continue;
			else
				j--;
		}
		for (k = j; k > 0; k--) {
			if (d >= synch[k - 1])
				break;

			peer_list[k] = peer_list[k - 1];
			error[k] = error[k - 1];
			synch[k] = synch[k - 1];
		}
		peer_list[k] = peer;
		error[k] = peer->jitter;
		synch[k] = d;
		j++;
	}
	nlist = j;

	/*
	 * If no survivors remain at this point, check if the local
	 * clock or modem drivers have been found. If so, nominate one
	 * of them as the only survivor. Otherwise, give up and leave
	 * the island to the rats.
	 */
	if (nlist == 0) {
		if (typeacts != 0) {
			typeacts->status = CTL_PST_SEL_DISTSYSPEER;
			peer_list[0] = typeacts;
			nlist = 1;
		} else if (typelocal != 0) {
			typelocal->status = CTL_PST_SEL_DISTSYSPEER;
			peer_list[0] = typelocal;
			nlist = 1;
		} else {
			if (osys_peer != NULL) {
				NLOG(NLOG_SYNCSTATUS)
				    msyslog(LOG_INFO,
				    "no servers reachable");
				report_event(EVNT_PEERSTCHG, NULL);
			}
		}
	}

	/*
	 * We can only trust the survivors if the number of candidates
	 * sys_minsane is at least the number required to detect and
	 * cast out one falsticker. For the Byzantine agreement
	 * algorithm used here, that number is 4; however, the default
	 * sys_minsane is 1 to speed initial synchronization. Careful
	 * operators will tinker a higher value and use at least that
	 * number of synchronization sources.
	 */
	if (nlist < sys_minsane)
		return;

	for (i = 0; i < nlist; i++)
		peer_list[i]->status = CTL_PST_SEL_SELCAND;

	/*
	 * Now, vote outlyers off the island by select jitter weighted
	 * by root distance. Continue voting as long as there are more
	 * than sys_minclock survivors and the minimum select jitter is
	 * greater than the maximum peer jitter. Stop if we are about to
	 * discard a TRUE or PREFER  peer, who of course has the
	 * immunity idol.
	 */
	while (1) {
		d = 1e9;
		e = -1e9;
		f = g = 0;
		k = 0;
		for (i = 0; i < nlist; i++) {
			if (error[i] < d)
				d = error[i];
			f = 0;
			if (nlist > 1) {
				for (j = 0; j < nlist; j++)
					f += DIFF(peer_list[j]->offset,
					    peer_list[i]->offset);
				f = SQRT(f / (nlist - 1));
			}
			if (f * synch[i] > e) {
				g = f;
				e = f * synch[i];
				k = i;
			}
		}
		f = max(f, LOGTOD(sys_precision));
		if (nlist <= sys_minclock || f <= d ||
		    peer_list[k]->flags & (FLAG_TRUE | FLAG_PREFER))
			break;
#ifdef DEBUG
		if (debug > 2)
			printf(
			    "select: drop %s select %.6f jitter %.6f\n",
			    ntoa(&peer_list[k]->srcadr), g, d);
#endif
		for (j = k + 1; j < nlist; j++) {
			peer_list[j - 1] = peer_list[j];
			error[j - 1] = error[j];
		}
		nlist--;
	}

	/*
	 * What remains is a list usually not greater than sys_minclock
	 * peers. We want only a peer at the lowest stratum to become
	 * the system peer, although all survivors are eligible for the
	 * combining algorithm. Consider each peer in turn and OR the
	 * leap bits on the assumption that, if some of them honk
	 * nonzero bits, they must know what they are doing. Check for
	 * prefer and pps peers at any stratum. Note that the head of
	 * the list is at the lowest stratum and that unsynchronized
	 * peers cannot survive this far.
	 */
	leap_next = 0;
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		sys_survivors++;
		leap_next |= peer->leap;
		peer->status = CTL_PST_SEL_SYNCCAND;
		if (peer->flags & FLAG_PREFER)
			sys_prefer = peer;
		if (peer == osys_peer)
			typesystem = peer;
#ifdef REFCLOCK
		if (peer->refclktype == REFCLK_ATOM_PPS)
			sys_pps = peer;
#endif /* REFCLOCK */
#if DEBUG
		if (debug > 1)
			printf("cluster: survivor %s metric %.6f\n",
			    ntoa(&peer_list[i]->srcadr), synch[i]);
#endif
	}

	/*
	 * Anticlockhop provision. Keep the current system peer if it is
	 * a survivor but not first in the list. But do that only HOPPER
	 * times.
	 */
	if (osys_peer == NULL || typesystem == NULL || typesystem ==
	    peer_list[0] || sys_hopper > sys_maxhop) {
		typesystem = peer_list[0];
		sys_hopper = 0;
	} else {
		peer->selbroken++;
	}

	/*
	 * Mitigation rules of the game. There are several types of
	 * peers that can be selected here: (1) orphan, (2) prefer peer
	 * (flag FLAG_PREFER) (3) pps peers (type REFCLK_ATOM_PPS), (4)
	 * the existing system peer, if any, and (5) the head of the
	 * survivor list.
	 */
	if (typesystem->stratum >= sys_orphan) {

		/*
		 * If in orphan mode, choose the system peer. If the
		 * lowest distance, we are the orphan parent and the
		 * offset is zero.
		 */
		sys_peer = typesystem;
		sys_peer->status = CTL_PST_SEL_SYSPEER;
		if (sys_orphandelay < sys_peer->rootdelay) {
			sys_offset = 0;
			sys_refid = htonl(LOOPBACKADR);
		} else {
			sys_offset = sys_peer->offset;
			sys_refid = addr2refid(&sys_peer->srcadr);
		}
		sys_jitter = LOGTOD(sys_precision);
#ifdef DEBUG
		if (debug > 1)
			printf("select: orphan offset %.6f\n",
			    sys_offset);
#endif
	} else if (sys_prefer) {

		/*
		 * If a pps peer is present, choose it; otherwise,
		 * choose the prefer peer.
		 */
		if (sys_pps) {
			sys_peer = sys_pps;
			sys_peer->status = CTL_PST_SEL_PPS;
			sys_offset = sys_peer->offset;
			if (!pps_control)
				NLOG(NLOG_SYSEVENT)
				    msyslog(LOG_INFO,
				    "pps sync enabled");
			pps_control = current_time;
#ifdef DEBUG
			if (debug > 1)
				printf("select: pps offset %.6f\n",
				    sys_offset);
#endif
		} else {
			sys_peer = sys_prefer;
			sys_peer->status = CTL_PST_SEL_SYSPEER;
			sys_offset = sys_peer->offset;
#ifdef DEBUG
			if (debug > 1)
				printf("select: prefer offset %.6f\n",
				    sys_offset);
#endif
		}
		if (sys_peer->stratum == STRATUM_REFCLOCK ||
		    sys_peer->stratum == STRATUM_UNSPEC)
			sys_refid = sys_peer->refid;
		else
			sys_refid = addr2refid(&sys_peer->srcadr);
		sys_jitter = sys_peer->jitter;
	} else {

		/*
		 * Otherwise, choose the anticlockhopper.
		 */ 
		sys_peer = typesystem;
		sys_peer->status = CTL_PST_SEL_SYSPEER;
		clock_combine(peer_list, nlist);
		if (sys_peer->stratum == STRATUM_REFCLOCK ||
		    sys_peer->stratum == STRATUM_UNSPEC)
			sys_refid = sys_peer->refid;
		else
			sys_refid = addr2refid(&sys_peer->srcadr);
		sys_jitter = SQRT(SQUARE(sys_peer->jitter) +
		    SQUARE(sys_jitter));
#ifdef DEBUG
		if (debug > 1)
			printf("select: combine offset %.6f\n",
			   sys_offset);
#endif
	}

	/*
	 * We have found the alpha male.
	 */
	sys_peer->flags |= FLAG_SYSPEER;
	if (osys_peer != sys_peer) {
		char *src;

		report_event(EVNT_PEERSTCHG, NULL);

#ifdef REFCLOCK
                if (sys_peer->flags & FLAG_REFCLOCK)
                        src = refnumtoa(&sys_peer->srcadr);
                else
#endif /* REFCLOCK */
                        src = ntoa(&sys_peer->srcadr);
		NLOG(NLOG_SYNCSTATUS)
		    msyslog(LOG_INFO, "synchronized to %s, stratum %d",
			src, sys_peer->stratum);
	}
	clock_update();
}


/*
 * clock_combine - compute system offset and jitter from selected peers
 */
static void
clock_combine(
	struct peer **peers,		/* survivor list */
	int	npeers			/* number of survivors */
	)
{
	int	i;
	double	x, y, z, w;

	y = z = w = 0;
	for (i = 0; i < npeers; i++) {
		x = root_distance(peers[i]);
		y += 1. / x;
		z += peers[i]->offset / x;
		w += SQUARE(peers[i]->offset - peers[0]->offset) / x;
	}
	sys_offset = z / y;
	sys_jitter = SQRT(w / y);
}

/*
 * root_distance - compute synchronization distance from peer to root
 */
static double
root_distance(
	struct peer *peer
	)
{
	double	dist;

	/*
	 * Careful squeak here. The value returned must be greater than
	 * the minimum root dispersion in order to avoid clockhop with
	 * highly precise reference clocks. In orphan mode lose the peer
	 * root delay, as that is used by the election algorithm.
	 */
	if (peer->stratum >= sys_orphan)
		dist = 0;
	else
		dist = peer->rootdelay;
	dist += max(sys_mindisp, dist + peer->delay) / 2 +
	    peer->rootdispersion + peer->disp + clock_phi *
	    (current_time - peer->update) + peer->jitter;
	return (dist);
}

/*
 * peer_xmit - send packet for persistent association.
 */
static void
peer_xmit(
	struct peer *peer	/* peer structure pointer */
	)
{
	struct pkt xpkt;	/* transmit packet */
	int	sendlen, authlen;
	keyid_t	xkeyid = 0;	/* transmit key ID */
	l_fp	xmt_tx;

	if (!peer->dstadr)	/* don't bother with peers without interface */
		return;

	/*
	 * This is deliciously complicated. There are three cases.
	 *
	 * case		leap	stratum	refid	delay	dispersion
	 *
	 * normal	system	system	system	system	system
	 * orphan child	00	orphan	system	orphan	system
	 * orphan parent 00	orphan	loopbk	0	0
	 */
	/*
	 * This is a normal packet. Use the system variables.
	 */
	if (sys_stratum < sys_orphan) {
		xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
		    peer->version, peer->hmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
		xpkt.refid = sys_refid;
		xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
		xpkt.rootdispersion =
		    HTONS_FP(DTOUFP(sys_rootdispersion));

	/*
	 * This is a orphan child packet. The host is synchronized to an
	 * orphan parent. Show leap synchronized, orphan stratum, system
	 * reference ID, orphan root delay and system root dispersion.
	 */
	} else if (sys_peer != NULL) {
		xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
		    peer->version, peer->hmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_orphan);
		xpkt.refid = htonl(LOOPBACKADR);
		xpkt.rootdelay = HTONS_FP(DTOFP(sys_orphandelay));
		xpkt.rootdispersion =
		    HTONS_FP(DTOUFP(sys_rootdispersion));

	/*
	 * This is an orphan parent. Show leap synchronized, orphan
	 * stratum, loopack reference ID and zero root delay and root
	 * dispersion.
	 */
	} else {
		xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
		    peer->version, peer->hmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_orphan);
		xpkt.refid = sys_refid;
		xpkt.rootdelay = 0;
		xpkt.rootdispersion = 0;
	}
	xpkt.ppoll = peer->hpoll;
	xpkt.precision = sys_precision;
	HTONL_FP(&sys_reftime, &xpkt.reftime);
	HTONL_FP(&peer->org, &xpkt.org);
	HTONL_FP(&peer->rec, &xpkt.rec);

	/*
	 * If the received packet contains a MAC, the transmitted packet
	 * is authenticated and contains a MAC. If not, the transmitted
	 * packet is not authenticated.
	 *
	 * It is most important when autokey is in use that the local
	 * interface IP address be known before the first packet is
	 * sent. Otherwise, it is not possible to compute a correct MAC
	 * the recipient will accept. Thus, the I/O semantics have to do
	 * a little more work. In particular, the wildcard interface
	 * might not be usable.
	 */
	sendlen = LEN_PKT_NOMAC;
	if (!(peer->flags & FLAG_AUTHENABLE)) {
		get_systime(&peer->xmt);
		HTONL_FP(&peer->xmt, &xpkt.xmt);
		sendpkt(&peer->srcadr, peer->dstadr, sys_ttl[peer->ttl],
			&xpkt, sendlen);
		peer->sent++;
#ifdef DEBUG
		if (debug)
			printf("transmit: at %ld %s->%s mode %d\n",
			       current_time, peer->dstadr ? stoa(&peer->dstadr->sin) : "-",
			       stoa(&peer->srcadr), peer->hmode);
#endif
		return;
	}

	/*
	 * The received packet contains a MAC, so the transmitted packet
	 * must be authenticated. If autokey is enabled, fuss with the
	 * various modes; otherwise, symmetric key cryptography is used.
	 */
#ifdef OPENSSL
	if (crypto_flags && (peer->flags & FLAG_SKEY)) {
		struct exten *exten;	/* extension field */

		/*
		 * The Public Key Dance (PKD): Cryptographic credentials
		 * are contained in extension fields, each including a
		 * 4-octet length/code word followed by a 4-octet
		 * association ID and optional additional data. Optional
		 * data includes a 4-octet data length field followed by
		 * the data itself. Request messages are sent from a
		 * configured association; response messages can be sent
		 * from a configured association or can take the fast
		 * path without ever matching an association. Response
		 * messages have the same code as the request, but have
		 * a response bit and possibly an error bit set. In this
		 * implementation, a message may contain no more than
		 * one command and no more than one response.
		 *
		 * Cryptographic session keys include both a public and
		 * a private componet. Request and response messages
		 * using extension fields are always sent with the
		 * private component set to zero. Packets without
		 * extension fields indlude the private component when
		 * the session key is generated.
		 */
		while (1) {
		
			/*
			 * Allocate and initialize a keylist if not
			 * already done. Then, use the list in inverse
			 * order, discarding keys once used. Keep the
			 * latest key around until the next one, so
			 * clients can use client/server packets to
			 * compute propagation delay.
			 *
			 * Note that once a key is used from the list,
			 * it is retained in the key cache until the
			 * next key is used. This is to allow a client
			 * to retrieve the encrypted session key
			 * identifier to verify authenticity.
			 *
			 * If for some reason a key is no longer in the
			 * key cache, a birthday has happened and the
			 * pseudo-random sequence is probably broken. In
			 * that case, purge the keylist and regenerate
			 * it.
			 */
			if (peer->keynumber == 0)
				make_keylist(peer, peer->dstadr);
			else
				peer->keynumber--;
			xkeyid = peer->keylist[peer->keynumber];
			if (authistrusted(xkeyid))
				break;
			else
				key_expire(peer);
		}
		peer->keyid = xkeyid;
		exten = NULL;
		switch (peer->hmode) {

			/*
			 * In broadcast server mode the autokey values are
			 * required by the broadcast clients. Push them when a
			 * new keylist is generated; otherwise, push the
			 * association message so the client can request them at
			 * other times.
			 */
		case MODE_BROADCAST:
			if (peer->flags & FLAG_ASSOC)
				exten = crypto_args(peer, CRYPTO_AUTO |
						    CRYPTO_RESP, NULL);
			else
				exten = crypto_args(peer, CRYPTO_ASSOC |
						    CRYPTO_RESP, NULL);
			break;

		/*
		 * In symmetric modes the digest, certificate, agreement
		 * parameters, cookie and autokey values are required.
		 * The leapsecond table is optional. But, a passive peer
		 * will not believe the active peer until the latter has
		 * synchronized, so the agreement must be postponed
		 * until then. In any case, if a new keylist is
		 * generated, the autokey values are pushed.
		 *
		 * If the crypto bit is lit, don't send requests.
		 */
		case MODE_ACTIVE:
		case MODE_PASSIVE:
			if (peer->flash & TEST9)
				break;
			/*
			 * Parameter and certificate.
			 */
			if (!peer->crypto)
				exten = crypto_args(peer, CRYPTO_ASSOC,
						    sys_hostname);
			else if (!(peer->crypto & CRYPTO_FLAG_VALID))
				exten = crypto_args(peer, CRYPTO_CERT,
						    peer->issuer);

			/*
			 * Identity. Note we have to sign the
			 * certificate before the cookie to avoid a
			 * deadlock when the passive peer is walking the
			 * certificate trail. Awesome.
			 */
			else if (!(peer->crypto & CRYPTO_FLAG_VRFY))
				exten = crypto_args(peer,
						    crypto_ident(peer), NULL);
			else if (sys_leap != LEAP_NOTINSYNC &&
				 !(peer->crypto & CRYPTO_FLAG_SIGN))
				exten = crypto_args(peer, CRYPTO_SIGN,
						    sys_hostname);

			/*
			 * Autokey. We request the cookie only when the
			 * server and client are synchronized and
			 * signatures work both ways. On the other hand,
			 * the active peer needs the autokey values
			 * before then and when the passive peer is
			 * waiting for the active peer to synchronize.
			 * Any time we regenerate the key list, we offer
			 * the autokey values without being asked.
			 */
			else if (sys_leap != LEAP_NOTINSYNC &&
				 peer->leap != LEAP_NOTINSYNC &&
				 !(peer->crypto & CRYPTO_FLAG_AGREE))
				exten = crypto_args(peer, CRYPTO_COOK,
						    NULL);
			else if (peer->flags & FLAG_ASSOC)
				exten = crypto_args(peer, CRYPTO_AUTO |
						    CRYPTO_RESP, NULL);
			else if (!(peer->crypto & CRYPTO_FLAG_AUTO))
				exten = crypto_args(peer, CRYPTO_AUTO,
						    NULL);

			/*
			 * Postamble. We trade leapseconds only when the
			 * server and client are synchronized.
			 */
			else if (sys_leap != LEAP_NOTINSYNC &&
				 peer->leap != LEAP_NOTINSYNC &&
				 peer->crypto & CRYPTO_FLAG_TAI &&
				 !(peer->crypto & CRYPTO_FLAG_LEAP))
				exten = crypto_args(peer, CRYPTO_TAI,
						    NULL);
			break;

		/*
		 * In client mode the digest, certificate, agreement
		 * parameters and cookie are required. The leapsecond
		 * table is optional. If broadcast client mode, the
		 * autokey values are required as well. In broadcast
		 * client mode, these values must be acquired during the
		 * client/server exchange to avoid having to wait until
		 * the next key list regeneration. Otherwise, the poor
		 * dude may die a lingering death until becoming
		 * unreachable and attempting rebirth.
		 *
		 * If neither the server or client have the agreement
		 * parameters, the protocol transmits the cookie in the
		 * clear. If the server has the parameters, the client
		 * requests them and the protocol blinds it using the
		 * agreed key. It is a protocol error if the client has
		 * the parameters but the server does not.
		 *
		 * If the crypto bit is lit, don't send requests.
		 */
		case MODE_CLIENT:
			if (peer->flash & TEST9)
				break;
			/*
			 * Parameter and certificate.
			 */
			if (!peer->crypto)
				exten = crypto_args(peer, CRYPTO_ASSOC,
						    sys_hostname);
			else if (!(peer->crypto & CRYPTO_FLAG_VALID))
				exten = crypto_args(peer, CRYPTO_CERT,
						    peer->issuer);

			/*
			 * Identity
			 */
			else if (!(peer->crypto & CRYPTO_FLAG_VRFY))
				exten = crypto_args(peer,
						    crypto_ident(peer), NULL);

			/*
			 * Autokey
			 */
			else if (!(peer->crypto & CRYPTO_FLAG_AGREE))
				exten = crypto_args(peer, CRYPTO_COOK,
						    NULL);
			else if (!(peer->crypto & CRYPTO_FLAG_AUTO) &&
				 (peer->cast_flags & MDF_BCLNT))
				exten = crypto_args(peer, CRYPTO_AUTO,
						    NULL);

			/*
			 * Postamble. We can sign the certificate here,
			 * since there is no chance of deadlock.
			 */
			else if (sys_leap != LEAP_NOTINSYNC &&
				 !(peer->crypto & CRYPTO_FLAG_SIGN))
				exten = crypto_args(peer, CRYPTO_SIGN,
						    sys_hostname);
			else if (sys_leap != LEAP_NOTINSYNC &&
				 peer->crypto & CRYPTO_FLAG_TAI &&
				 !(peer->crypto & CRYPTO_FLAG_LEAP))
				exten = crypto_args(peer, CRYPTO_TAI,
						    NULL);
			break;
		}

		/*
		 * Build the extension fields as directed. A response to
		 * a request is always sent, even if an error. If an
		 * error occurs when sending a request, the crypto
		 * machinery broke or was misconfigured. In that case
		 * light the crypto bit to suppress further requests.
		 */
		if (peer->cmmd != NULL) {
			peer->cmmd->associd = htonl(peer->associd);
			sendlen += crypto_xmit(&xpkt, &peer->srcadr,
					       sendlen, peer->cmmd, 0);
			free(peer->cmmd);
			peer->cmmd = NULL;
		}
		if (exten != NULL) {
			int ltemp = 0;

			if (exten->opcode != 0) {
				ltemp = crypto_xmit(&xpkt,
						       &peer->srcadr, sendlen, exten, 0);
				if (ltemp == 0) {
					peer->flash |= TEST9; /* crypto error */
					free(exten);
					return;
				}
			}
			sendlen += ltemp;
			free(exten);
		}

		/*
		 * If extension fields are present, we must use a
		 * private cookie value of zero. Don't send if the
		 * crypto bit is set and no extension field is present,
		 * but in that case give back the key. Most intricate.
		 */
		if (sendlen > LEN_PKT_NOMAC) {
			session_key(&peer->dstadr->sin, &peer->srcadr,
			    xkeyid, 0, 2);
		} else if (peer->flash & TEST9) {
			authtrust(xkeyid, 0);
			return;
		}
	} 
#endif /* OPENSSL */

	/*
	 * Stash the transmit timestamp corrected for the encryption
	 * delay. If autokey, give back the key, as we use keys only
	 * once. Check for errors such as missing keys, buffer overflow,
	 * etc.
	 */
	xkeyid = peer->keyid;
	get_systime(&peer->xmt);
	L_ADD(&peer->xmt, &sys_authdelay);
	HTONL_FP(&peer->xmt, &xpkt.xmt);
	authlen = authencrypt(xkeyid, (u_int32 *)&xpkt, sendlen);
	if (authlen == 0) {
		msyslog(LOG_INFO, "transmit: %s key %u not found",
		    stoa(&peer->srcadr), xkeyid);
		peer->flash |= TEST9;		/* no key found */
		return;
	}
	sendlen += authlen;
#ifdef OPENSSL
	if (xkeyid > NTP_MAXKEY)
		authtrust(xkeyid, 0);
#endif /* OPENSSL */
	get_systime(&xmt_tx);
	if (sendlen > sizeof(xpkt)) {
		msyslog(LOG_ERR, "buffer overflow %u", sendlen);
		exit (-1);
	}
	sendpkt(&peer->srcadr, peer->dstadr, sys_ttl[peer->ttl], &xpkt,
		sendlen);

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
#ifdef OPENSSL
#ifdef DEBUG
	if (debug)
		printf(
			"transmit: at %ld %s->%s mode %d keyid %08x len %d mac %d index %d\n",
			current_time, peer->dstadr ? ntoa(&peer->dstadr->sin) : "-",
			ntoa(&peer->srcadr), peer->hmode, xkeyid, sendlen -
			authlen, authlen, peer->keynumber);
#endif
#else
#ifdef DEBUG
	if (debug)
		printf(
			"transmit: at %ld %s->%s mode %d keyid %08x len %d mac %d\n",
			current_time, peer->dstadr ? ntoa(&peer->dstadr->sin) : "-",
			ntoa(&peer->srcadr), peer->hmode, xkeyid, sendlen -
			authlen, authlen);
#endif
#endif /* OPENSSL */
}


/*
 * fast_xmit - Send packet for nonpersistent association. Note that
 * neither the source or destination can be a broadcast address.
 */
static void
fast_xmit(
	struct recvbuf *rbufp,	/* receive packet pointer */
	int	xmode,		/* transmit mode */
	keyid_t	xkeyid,		/* transmit key ID */
	int	mask		/* restrict mask */
	)
{
	struct pkt xpkt;		/* transmit packet structure */
	struct pkt *rpkt;		/* receive packet structure */
	l_fp	xmt_ts;			/* timestamp */
	l_fp	xmt_tx;			/* timestamp after authent */
	int	sendlen, authlen;
#ifdef OPENSSL
	u_int32	temp32;
#endif

	/*
	 * Initialize transmit packet header fields from the receive
	 * buffer provided. We leave some fields intact as received. If
	 * the gazinta was from a multicast address, the gazoutta must
	 * go out another way.
	 *
	 * The root delay field is special. If the system stratum is
	 * less than the orphan stratum, send the real root delay.
	 * Otherwise, if there is no system peer, send the orphan delay.
	 * Otherwise, we must be an orphan parent, so send zero.
	 */
	rpkt = &rbufp->recv_pkt;
	if (rbufp->dstadr->flags & INT_MCASTOPEN)
		rbufp->dstadr = findinterface(&rbufp->recv_srcadr);

	/*
	 * This is deliciously complicated. There are four cases.
	 *
	 * case		leap	stratum	refid	delay	dispersion
	 *
	 * KoD		11	16	KISS	system	system
	 * normal	system	system	system	system	system
	 * orphan child	00	orphan	system	orphan	system
	 * orphan parent 00	orphan	loopbk	0	0
	 */
	/*
	 * This is a kiss-of-death (KoD) packet. Show leap
	 * unsynchronized, stratum zero, reference ID the four-character
	 * kiss code and system root delay. Note the rate limit on these
	 * packets. Once a second initialize a bucket counter. Every
	 * packet sent decrements the counter until reaching zero. If
	 * the counter is zero, drop the kiss.
	 */
	if (mask & RES_LIMITED) {
		sys_limitrejected++;
		if (sys_kod == 0 || !(mask & RES_DEMOBILIZE))
			return;

		sys_kod--;
		xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOTINSYNC,
		    PKT_VERSION(rpkt->li_vn_mode), xmode);
		xpkt.stratum = STRATUM_UNSPEC;
		memcpy(&xpkt.refid, "RATE", 4);
		xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
		xpkt.rootdispersion =
		    HTONS_FP(DTOUFP(sys_rootdispersion));

	/*
	 * This is a normal packet. Use the system variables.
	 */
	} else if (sys_stratum < sys_orphan) {
		xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
		    PKT_VERSION(rpkt->li_vn_mode), xmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
		xpkt.refid = sys_refid;
		xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
		xpkt.rootdispersion =
		    HTONS_FP(DTOUFP(sys_rootdispersion));

	/*
	 * This is a orphan child packet. The host is synchronized to an
	 * orphan parent. Show leap synchronized, orphan stratum, system
	 * reference ID and orphan root delay.
	 */
	} else if (sys_peer != NULL) {
		xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
		    PKT_VERSION(rpkt->li_vn_mode), xmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_orphan);
		xpkt.refid = sys_refid;
		xpkt.rootdelay = HTONS_FP(DTOFP(sys_orphandelay));
		xpkt.rootdispersion =
		    HTONS_FP(DTOUFP(sys_rootdispersion));

	/*
	 * This is an orphan parent. Show leap synchronized, orphan
	 * stratum, loopack reference ID and zero root delay.
	 */
	} else {
		xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOWARNING,
		    PKT_VERSION(rpkt->li_vn_mode), xmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_orphan);
		xpkt.refid = htonl(LOOPBACKADR);
		xpkt.rootdelay = HTONS_FP(DTOFP(0));
		xpkt.rootdispersion = HTONS_FP(DTOFP(0));
	}
	xpkt.ppoll = rpkt->ppoll;
	xpkt.precision = sys_precision;
	xpkt.rootdispersion = HTONS_FP(DTOUFP(sys_rootdispersion));
	HTONL_FP(&sys_reftime, &xpkt.reftime);
	xpkt.org = rpkt->xmt;
	HTONL_FP(&rbufp->recv_time, &xpkt.rec);

	/*
	 * If the received packet contains a MAC, the transmitted packet
	 * is authenticated and contains a MAC. If not, the transmitted
	 * packet is not authenticated.
	 */
	sendlen = LEN_PKT_NOMAC;
	if (rbufp->recv_length == sendlen) {
		get_systime(&xmt_ts);
		HTONL_FP(&xmt_ts, &xpkt.xmt);
		sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, 0, &xpkt,
		    sendlen);
#ifdef DEBUG
		if (debug)
			printf("transmit: at %ld %s->%s mode %d\n",
			    current_time, stoa(&rbufp->dstadr->sin),
			    stoa(&rbufp->recv_srcadr), xmode);
#endif
		return;
	}

	/*
	 * The received packet contains a MAC, so the transmitted packet
	 * must be authenticated. For symmetric key cryptography, use
	 * the predefined and trusted symmetric keys to generate the
	 * cryptosum. For autokey cryptography, use the server private
	 * value to generate the cookie, which is unique for every
	 * source-destination-key ID combination.
	 */
#ifdef OPENSSL
	if (xkeyid > NTP_MAXKEY) {
		keyid_t cookie;

		/*
		 * The only way to get here is a reply to a legitimate
		 * client request message, so the mode must be
		 * MODE_SERVER. If an extension field is present, there
		 * can be only one and that must be a command. Do what
		 * needs, but with private value of zero so the poor
		 * jerk can decode it. If no extension field is present,
		 * use the cookie to generate the session key.
		 */
		cookie = session_key(&rbufp->recv_srcadr,
		    &rbufp->dstadr->sin, 0, sys_private, 0);
		if (rbufp->recv_length >= (int)(sendlen + MAX_MAC_LEN +
		    2 * sizeof(u_int32))) {
			session_key(&rbufp->dstadr->sin,
			    &rbufp->recv_srcadr, xkeyid, 0, 2);
			temp32 = CRYPTO_RESP;
			rpkt->exten[0] |= htonl(temp32);
			sendlen += crypto_xmit(&xpkt,
			    &rbufp->recv_srcadr, sendlen,
			    (struct exten *)rpkt->exten, cookie);
		} else {
			session_key(&rbufp->dstadr->sin,
			    &rbufp->recv_srcadr, xkeyid, cookie, 2);
		}
	}
#endif /* OPENSSL */
	get_systime(&xmt_ts);
	L_ADD(&xmt_ts, &sys_authdelay);
	HTONL_FP(&xmt_ts, &xpkt.xmt);
	authlen = authencrypt(xkeyid, (u_int32 *)&xpkt, sendlen);
	sendlen += authlen;
#ifdef OPENSSL
	if (xkeyid > NTP_MAXKEY)
		authtrust(xkeyid, 0);
#endif /* OPENSSL */
	get_systime(&xmt_tx);
	if (sendlen > sizeof(xpkt)) {
		msyslog(LOG_ERR, "buffer overflow %u", sendlen);
		exit (-1);
	}
	sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, 0, &xpkt, sendlen);

	/*
	 * Calculate the encryption delay. Keep the minimum over the
	 * latest two samples.
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
		    "transmit: at %ld %s->%s mode %d keyid %08x len %d mac %d\n",
		    current_time, ntoa(&rbufp->dstadr->sin),
		    ntoa(&rbufp->recv_srcadr), xmode, xkeyid, sendlen -
		    authlen, authlen);
#endif
}


#ifdef OPENSSL
/*
 * key_expire - purge the key list
 */
void
key_expire(
	struct peer *peer	/* peer structure pointer */
	)
{
	int i;

	if (peer->keylist != NULL) {
		for (i = 0; i <= peer->keynumber; i++)
			authtrust(peer->keylist[i], 0);
		free(peer->keylist);
		peer->keylist = NULL;
	}
	value_free(&peer->sndval);
	peer->keynumber = 0;
#ifdef DEBUG
	if (debug)
		printf("key_expire: at %lu\n", current_time);
#endif
}
#endif /* OPENSSL */


/*
 * Determine if the peer is unfit for synchronization
 *
 * A peer is unfit for synchronization if
 * > TEST10 bad leap or stratum below floor or at or above ceiling
 * > TEST11 root distance exceeded
 * > TEST12 a direct or indirect synchronization loop would form
 * > TEST13 unreachable or noselect
 */
int				/* FALSE if fit, TRUE if unfit */
peer_unfit(
	struct peer *peer	/* peer structure pointer */
	)
{
	int	rval = 0;

	/*
	 * A stratum error occurs if (1) the server has never been
	 * synchronized, (2) the server stratum is below the floor or
	 * greater than or equal to the ceiling, (3) the system stratum
	 * is below the orphan stratum and the server stratum is greater
	 * than or equal to the orphan stratum.
	 */
	if (peer->leap == LEAP_NOTINSYNC || peer->stratum < sys_floor ||
	    peer->stratum >= sys_ceiling || (sys_stratum < sys_orphan &&
	    peer->stratum >= sys_orphan))
		rval |= TEST10;		/* stratum out of bounds */

	/*
	 * A distance error occurs if the root distance is greater than
	 * or equal to the distance threshold plus the increment due to
	 * one poll interval.
	 */
	if (root_distance(peer) >= sys_maxdist + clock_phi *
	    ULOGTOD(sys_poll))
		rval |= TEST11;		/* distance exceeded */

	/*
	 * A loop error occurs if the remote peer is synchronized to the
	 * local peer of if the remote peer is synchronized to the same
	 * server as the local peer, but only if the remote peer is not
	 * the orphan parent.
	 */
	if (peer->stratum > 1 && peer->refid != htonl(LOOPBACKADR) &&
	    ((!peer->dstadr || peer->refid == peer->dstadr->addr_refid) ||
	    peer->refid == sys_refid))
		rval |= TEST12;		/* synch loop */

	/*
	 * An unreachable error occurs if the server is unreachable or
	 * the noselect bit is set.
	 */
	if (!peer->reach || peer->flags & FLAG_NOSELECT)
		rval |= TEST13;		/* unreachable */

	peer->flash &= ~PEER_TEST_MASK;
	peer->flash |= rval;
	return (rval);
}


/*
 * Find the precision of this particular machine
 */
#define MINSTEP 100e-9		/* minimum clock increment (s) */
#define MAXSTEP 20e-3		/* maximum clock increment (s) */
#define MINLOOPS 5		/* minimum number of step samples */

/*
 * This routine calculates the system precision, defined as the minimum
 * of a sequence of differences between successive readings of the
 * system clock. However, if the system clock can be read more than once
 * during a tick interval, the difference can be zero or one LSB unit,
 * where the LSB corresponds to one nanosecond or one microsecond.
 * Conceivably, if some other process preempts this one and reads the
 * clock, the difference can be more than one LSB unit.
 *
 * For hardware clock frequencies of 10 MHz or less, we assume the
 * logical clock advances only at the hardware clock tick. For higher
 * frequencies, we assume the logical clock can advance no more than 100
 * nanoseconds between ticks.
 */
int
default_get_precision(void)
{
	l_fp	val;		/* current seconds fraction */
	l_fp	last;		/* last seconds fraction */
	l_fp	diff;		/* difference */
	double	tick;		/* computed tick value */
	double	dtemp;		/* scratch */
	int	i;		/* log2 precision */

	/*
	 * Loop to find tick value in nanoseconds. Toss out outlyer
	 * values less than the minimun tick value. In wacky cases, use
	 * the default maximum value.
	 */
	get_systime(&last);
	tick = MAXSTEP;
	for (i = 0; i < MINLOOPS;) {
		get_systime(&val);
		diff = val;
		L_SUB(&diff, &last);
		last = val;
		LFPTOD(&diff, dtemp);
		if (dtemp < MINSTEP)
			continue;
		i++;
		if (dtemp < tick)
			tick = dtemp;
	}

	/*
	 * Find the nearest power of two.
	 */
	NLOG(NLOG_SYSEVENT)
	    msyslog(LOG_INFO, "precision = %.3f usec", tick * 1e6);
	for (i = 0; tick <= 1; i++)
		tick *= 2;
	if (tick - 1. > 1. - tick / 2)
		i--;
	return (-i);
}


/*
 * kod_proto - called once per second to limit kiss-of-death packets
 */
void
kod_proto(void)
{
	sys_kod = sys_kod_rate;
}


/*
 * init_proto - initialize the protocol module's data
 */
void
init_proto(void)
{
	l_fp	dummy;
	int	i;

	/*
	 * Fill in the sys_* stuff.  Default is don't listen to
	 * broadcasting, authenticate.
	 */
	sys_leap = LEAP_NOTINSYNC;
	sys_stratum = STRATUM_UNSPEC;
	memcpy(&sys_refid, "INIT", 4);
	sys_precision = (s_char)default_get_precision();
	sys_jitter = LOGTOD(sys_precision);
	sys_rootdelay = 0;
	sys_orphandelay = (double)(ntp_random() & 0xffff) / 65536. *
	    sys_maxdist;
	sys_rootdispersion = 0;
	L_CLR(&sys_reftime);
	sys_peer = NULL;
	sys_survivors = 0;
	get_systime(&dummy);
	sys_manycastserver = 0;
	sys_bclient = 0;
	sys_bdelay = DEFBROADDELAY;
	sys_calldelay = BURST_DELAY;
	sys_authenticate = 1;
	L_CLR(&sys_authdelay);
	sys_authdly[0] = sys_authdly[1] = 0;
	sys_stattime = 0;
	proto_clr_stats();
	for (i = 0; i < MAX_TTL; i++) {
		sys_ttl[i] = (u_char)((i * 256) / MAX_TTL);
		sys_ttlmax = i;
	}
#ifdef OPENSSL
	sys_automax = 1 << NTP_AUTOMAX;
#endif /* OPENSSL */

	/*
	 * Default these to enable
	 */
	ntp_enable = 1;
#ifndef KERNEL_FLL_BUG
	kern_enable = 1;
#endif
	pps_enable = 0;
	stats_control = 1;
}


/*
 * proto_config - configure the protocol module
 */
void
proto_config(
	int	item,
	u_long	value,
	double	dvalue,
	struct sockaddr_storage* svalue
	)
{
	/*
	 * Figure out what he wants to change, then do it
	 */
	switch (item) {

	/*
	 * Turn on/off kernel discipline.
	 */
	case PROTO_KERNEL:
		kern_enable = (int)value;
		break;

	/*
	 * Turn on/off clock discipline.
	 */
	case PROTO_NTP:
		ntp_enable = (int)value;
		break;

	/*
	 * Turn on/off monitoring.
	 */
	case PROTO_MONITOR:
		if (value)
			mon_start(MON_ON);
		else
			mon_stop(MON_ON);
		break;

	/*
	 * Turn on/off statistics.
	 */
	case PROTO_FILEGEN:
		stats_control = (int)value;
		break;

	/*
	 * Turn on/off enable broadcasts.
	 */
	case PROTO_BROADCLIENT:
		sys_bclient = (int)value;
		if (sys_bclient == 0)
			io_unsetbclient();
		else
			io_setbclient();
		break;

	/*
	 * Turn on/off PPS discipline.
	 */
	case PROTO_PPS:
		pps_enable = (int)value;
		break;

	/*
	 * Add muliticast group address.
	 */
	case PROTO_MULTICAST_ADD:
		if (svalue)
		    io_multicast_add(*svalue);
		sys_bclient = 1;
		break;

	/*
	 * Delete multicast group address.
	 */
	case PROTO_MULTICAST_DEL:
		if (svalue)
		    io_multicast_del(*svalue);
		break;

	/*
	 * Set default broadcast delay.
	 */
	case PROTO_BROADDELAY:
		sys_bdelay = dvalue;
		break;

	/*
	 * Set modem call delay.
	 */
	case PROTO_CALLDELAY:
		sys_calldelay = (int)value;
		break;

	/*
	 * Turn on/off authentication to mobilize ephemeral
	 * associations.
	 */
	case PROTO_AUTHENTICATE:
		sys_authenticate = (int)value;
		break;

	/*
	 * Set minimum number of survivors.
	 */
	case PROTO_MINCLOCK:
		sys_minclock = (int)dvalue;
		break;

	/*
	 * Set maximum number of preemptable associations.
	 */
	case PROTO_MAXCLOCK:
		sys_maxclock = (int)dvalue;
		break;

	/*
	 * Set minimum number of survivors.
	 */
	case PROTO_MINSANE:
		sys_minsane = (int)dvalue;
		break;

	/*
	 * Set stratum floor.
	 */
	case PROTO_FLOOR:
		sys_floor = (int)dvalue;
		break;

	/*
	 * Set stratum ceiling.
	 */
	case PROTO_CEILING:
		sys_ceiling = (int)dvalue;
		break;

	/*
	 * Set orphan stratum.
	 */
	case PROTO_ORPHAN:
		sys_orphan = (int)dvalue;
		break;

	/*
	 * Set cohort switch.
	 */
	case PROTO_COHORT:
		sys_cohort = (int)dvalue;
		break;

	/*
	 * Set minimum dispersion increment.
	 */
	case PROTO_MINDISP:
		sys_mindisp = dvalue;
		break;

	/*
	 * Set maximum distance (select threshold).
	 */
	case PROTO_MAXDIST:
		sys_maxdist = dvalue;
		break;

	/*
	 * Set anticlockhop threshold.
	 */
	case PROTO_MAXHOP:
		sys_maxhop = (int)dvalue;
		break;

	/*
	 * Set adjtime() resolution (s).
	 */
	case PROTO_ADJ:
		sys_tick = dvalue;
		break;

	/*
	 * Set manycast beacon interval.
	 */
	case PROTO_BEACON:
		sys_beacon = (int)dvalue;
		break;

#ifdef REFCLOCK
	/*
	 * Turn on/off refclock calibrate
	 */
	case PROTO_CAL:
		cal_enable = (int)value;
		break;
#endif /* REFCLOCK */
	default:

		/*
		 * Log this error.
		 */
		msyslog(LOG_INFO,
		    "proto_config: illegal item %d, value %ld", item,
		    value);
	}
}


/*
 * proto_clr_stats - clear protocol stat counters
 */
void
proto_clr_stats(void)
{
	sys_stattime = current_time;
	sys_received = 0;
	sys_processed = 0;
	sys_newversionpkt = 0;
	sys_oldversionpkt = 0;
	sys_unknownversion = 0;
	sys_restricted = 0;
	sys_badlength = 0;
	sys_badauth = 0;
	sys_limitrejected = 0;
}
