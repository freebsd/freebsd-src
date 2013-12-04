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
#ifdef HAVE_LIBSCF_H
#include <libscf.h>
#include <unistd.h>
#endif /* HAVE_LIBSCF_H */


#if defined(VMS) && defined(VMS_LOCALUNIT)	/*wjm*/
#include "ntp_refclock.h"
#endif

/*
 * This macro defines the authentication state. If x is 1 authentication
 * is required; othewise it is optional.
 */
#define	AUTH(x, y)	((x) ? (y) == AUTH_OK : (y) == AUTH_OK || \
			    (y) == AUTH_NONE)

#define	AUTH_NONE	0	/* authentication not required */
#define	AUTH_OK		1	/* authentication OK */
#define	AUTH_ERROR	2	/* authentication error */
#define	AUTH_CRYPTO	3	/* crypto_NAK */

/*
 * traffic shaping parameters
 */
#define	NTP_IBURST	6	/* packets in iburst */
#define	RESP_DELAY	1	/* refclock burst delay (s) */

/*
 * System variables are declared here. Unless specified otherwise, all
 * times are in seconds.
 */
u_char	sys_leap;		/* system leap indicator */
u_char	sys_stratum;		/* system stratum */
s_char	sys_precision;		/* local clock precision (log2 s) */
double	sys_rootdelay;		/* roundtrip delay to primary source */
double	sys_rootdisp;		/* dispersion to primary source */
u_int32 sys_refid;		/* reference id (network byte order) */
l_fp	sys_reftime;		/* last update time */
struct	peer *sys_peer;		/* current peer */

/*
 * Rate controls. Leaky buckets are used to throttle the packet
 * transmission rates in order to protect busy servers such as at NIST
 * and USNO. There is a counter for each association and another for KoD
 * packets. The association counter decrements each second, but not
 * below zero. Each time a packet is sent the counter is incremented by
 * a configurable value representing the average interval between
 * packets. A packet is delayed as long as the counter is greater than
 * zero. Note this does not affect the time value computations.
 */
/*
 * Nonspecified system state variables
 */
int	sys_bclient;		/* broadcast client enable */
double	sys_bdelay;		/* broadcast client default delay */
int	sys_authenticate;	/* requre authentication for config */
l_fp	sys_authdelay;		/* authentication delay */
double	sys_offset;	/* current local clock offset */
double	sys_mindisp = MINDISPERSE; /* minimum distance (s) */
double	sys_maxdist = MAXDISTANCE; /* selection threshold */
double	sys_jitter;		/* system jitter */
u_long	sys_epoch;		/* last clock update time */
static	double sys_clockhop;	/* clockhop threshold */
int	leap_tai;		/* TAI at next next leap */
u_long	leap_sec;		/* next scheduled leap from file */
u_long	leap_peers;		/* next scheduled leap from peers */
u_long	leap_expire;		/* leap information expiration */
static int leap_vote;		/* leap consensus */
keyid_t	sys_private;		/* private value for session seed */
int	sys_manycastserver;	/* respond to manycast client pkts */
int	peer_ntpdate;		/* active peers in ntpdate mode */
int	sys_survivors;		/* truest of the truechimers */

/*
 * TOS and multicast mapping stuff
 */
int	sys_floor = 0;		/* cluster stratum floor */
int	sys_ceiling = STRATUM_UNSPEC; /* cluster stratum ceiling */
int	sys_minsane = 1;	/* minimum candidates */
int	sys_minclock = NTP_MINCLOCK; /* minimum candidates */
int	sys_maxclock = NTP_MAXCLOCK; /* maximum candidates */
int	sys_cohort = 0;		/* cohort switch */
int	sys_orphan = STRATUM_UNSPEC + 1; /* orphan stratum */
int	sys_beacon = BEACON;	/* manycast beacon interval */
int	sys_ttlmax;		/* max ttl mapping vector index */
u_char	sys_ttl[MAX_TTL];	/* ttl mapping vector */

/*
 * Statistics counters - first the good, then the bad
 */
u_long	sys_stattime;		/* elapsed time */
u_long	sys_received;		/* packets received */
u_long	sys_processed;		/* packets for this host */
u_long	sys_newversion;		/* current version */
u_long	sys_oldversion;		/* old version */
u_long	sys_restricted;		/* access denied */
u_long	sys_badlength;		/* bad length or format */
u_long	sys_badauth;		/* bad authentication */
u_long	sys_declined;		/* declined */
u_long	sys_limitrejected;	/* rate exceeded */
u_long	sys_kodsent;		/* KoD sent */

static	double	root_distance	(struct peer *);
static	void	clock_combine	(struct peer **, int);
static	void	peer_xmit	(struct peer *);
static	void	fast_xmit	(struct recvbuf *, int, keyid_t,
				    int);
static	void	clock_update	(struct peer *);
static	int	default_get_precision (void);
static	int	local_refid	(struct peer *);
static	int	peer_unfit	(struct peer *);


/*
 * transmit - transmit procedure called by poll timeout
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
	hpoll = peer->hpoll;

	/*
	 * In broadcast mode the poll interval is never changed from
	 * minpoll.
	 */
	if (peer->cast_flags & (MDF_BCAST | MDF_MCAST)) {
		peer->outdate = current_time;
		if (sys_leap != LEAP_NOTINSYNC)
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
		    peer_associations < sys_maxclock) {
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
		peer->unreach++;
		peer->reach <<= 1;
		if (!(peer->reach & 0x0f))
			clock_filter(peer, 0., 0., MAXDISPERSE);
		if (!peer->reach) {

			/*
			 * Here the peer is unreachable. If it was
			 * previously reachable raise a trap. Send a
			 * burst if enabled.
			 */
			if (oreach)
				report_event(PEVNT_UNREACH, peer, NULL);
			if ((peer->flags & FLAG_IBURST) &&
			    peer->retry == 0)
				peer->retry = NTP_RETRY;
		} else {

			/*
			 * Here the peer is reachable. Send a burst if
			 * enabled and the peer is fit.
			 */
			hpoll = sys_poll;
			if (!(peer->flags & FLAG_PREEMPT &&
			    peer->hmode == MODE_CLIENT))
				peer->unreach = 0;
			if ((peer->flags & FLAG_BURST) && peer->retry ==
			    0 && !peer_unfit(peer))
				peer->retry = NTP_RETRY;
		}

		/*
		 * Watch for timeout. If preemptable, toss the rascal;
		 * otherwise, bump the poll interval. Note the
		 * poll_update() routine will clamp it to maxpoll.
		 */ 
		if (peer->unreach >= NTP_UNREACH) {
			hpoll++;
			if (peer->flags & FLAG_PREEMPT) {
				report_event(PEVNT_RESTART, peer,
				    "timeout");
				if (peer->hmode != MODE_CLIENT) {
					peer_clear(peer, "TIME");
					unpeer(peer);
					return;
				}
				if (peer_associations > sys_maxclock &&
				    score_all(peer)) {
					peer_clear(peer, "TIME");
					unpeer(peer);
					return;
				}
			}
		}
	} else {
		peer->burst--;
		if (peer->burst == 0) {

			/*
			 * If ntpdate mode and the clock has not been
			 * set and all peers have completed the burst,
			 * we declare a successful failure.
			 */
			if (mode_ntpdate) {
				peer_ntpdate--;
				if (peer_ntpdate == 0) {
					msyslog(LOG_NOTICE,
					    "ntpd: no servers found");
					printf(
					    "ntpd: no servers found\n");
					exit (0);
				}
			}
		}
	}
	if (peer->retry > 0)
		peer->retry--;

	/*
	 * Do not transmit if in broadcast client mode. 
	 */
	if (peer->hmode != MODE_BCLIENT)
		peer_xmit(peer);
	poll_update(peer, hpoll);
}


/*
 * receive - receive procedure called for each packet received
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
	int	retcode = AM_NOMATCH;	/* match code */
	keyid_t	skeyid = 0;		/* key IDs */
	u_int32	opcode = 0;		/* extension field opcode */
	sockaddr_u *dstadr_sin; 	/* active runway */
	struct peer *peer2;		/* aux peer structure pointer */
	endpt *	match_ep;		/* newpeer() local address */
	l_fp	p_org;			/* origin timestamp */
	l_fp	p_rec;			/* receive timestamp */
	l_fp	p_xmt;			/* transmit timestamp */
#ifdef OPENSSL
	struct autokey *ap;		/* autokey structure pointer */
	int	rval;			/* cookie snatcher */
	keyid_t	pkeyid = 0, tkeyid = 0;	/* key IDs */
#endif /* OPENSSL */
#ifdef HAVE_NTP_SIGND
	static unsigned char zero_key[16];
#endif /* HAVE_NTP_SIGND */

	/*
	 * Monitor the packet and get restrictions. Note that the packet
	 * length for control and private mode packets must be checked
	 * by the service routines. Some restrictions have to be handled
	 * later in order to generate a kiss-o'-death packet.
	 */
	/*
	 * Bogus port check is before anything, since it probably
	 * reveals a clogging attack.
	 */
	sys_received++;
	if (SRCPORT(&rbufp->recv_srcadr) < NTP_PORT) {
		sys_badlength++;
		return;				/* bogus port */
	}
	restrict_mask = restrictions(&rbufp->recv_srcadr);
#ifdef DEBUG
	if (debug > 1)
		printf("receive: at %ld %s<-%s flags %x restrict %03x\n",
		    current_time, stoa(&rbufp->dstadr->sin),
		    stoa(&rbufp->recv_srcadr),
		    rbufp->dstadr->flags, restrict_mask);
#endif
	pkt = &rbufp->recv_pkt;
	hisversion = PKT_VERSION(pkt->li_vn_mode);
	hisleap = PKT_LEAP(pkt->li_vn_mode);
	hismode = (int)PKT_MODE(pkt->li_vn_mode);
	hisstratum = PKT_TO_STRATUM(pkt->stratum);
	if (restrict_mask & RES_IGNORE) {
		sys_restricted++;
		return;				/* ignore everything */
	}
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
		return;				/* no time serve */
	}

	/*
	 * This is for testing. If restricted drop ten percent of
	 * surviving packets.
	 */
	if (restrict_mask & RES_TIMEOUT) {
		if ((double)ntp_random() / 0x7fffffff < .1) {
			sys_restricted++;
			return;			/* no flakeway */
		}
	}
	
	/*
	 * Version check must be after the query packets, since they
	 * intentionally use an early version.
	 */
	if (hisversion == NTP_VERSION) {
		sys_newversion++;		/* new version */
	} else if (!(restrict_mask & RES_VERSION) && hisversion >=
	    NTP_OLDVERSION) {
		sys_oldversion++;		/* previous version */
	} else {
		sys_badlength++;
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
	 * with DES; if 5, the packet is authenticated with MD5; if 6,
	 * the packet is authenticated with SHA. If 2 or * 4, the packet
	 * is a runt and discarded forthwith. If greater than 6, an
	 * extension field is present, so we subtract the length of the
	 * field and go around again.
	 */
	authlen = LEN_PKT_NOMAC;
	has_mac = rbufp->recv_length - authlen;
	while (has_mac != 0) {
		u_int32	len;

		if (has_mac % 4 != 0 || has_mac < MIN_MAC_LEN) {
			sys_badlength++;
			return;			/* bad length */
		}
		if (has_mac <= MAX_MAC_LEN) {
			skeyid = ntohl(((u_int32 *)pkt)[authlen / 4]);
			break;

		} else {
			opcode = ntohl(((u_int32 *)pkt)[authlen / 4]);
 			len = opcode & 0xffff;
			if (len % 4 != 0 || len < 4 || len + authlen >
			    rbufp->recv_length) {
				sys_badlength++;
				return;		/* bad length */
			}
			authlen += len;
			has_mac -= len;
		}
	}

	/*
	 * If authentication required, a MAC must be present.
	 */
	if (restrict_mask & RES_DONTTRUST && has_mac == 0) {
		sys_restricted++;
		return;				/* access denied */
	}

	/*
	 * Update the MRU list and finger the cloggers. It can be a
	 * little expensive, so turn it off for production use.
	 */
	restrict_mask = ntp_monitor(rbufp, restrict_mask);
	if (restrict_mask & RES_LIMITED) {
		sys_limitrejected++;
		if (!(restrict_mask & RES_KOD) || MODE_BROADCAST ==
		    hismode || MODE_SERVER == hismode)
			return;			/* rate exceeded */

		if (hismode == MODE_CLIENT)
			fast_xmit(rbufp, MODE_SERVER, skeyid,
			    restrict_mask);
		else
			fast_xmit(rbufp, MODE_ACTIVE, skeyid,
			    restrict_mask);
		return;				/* rate exceeded */
	}
	restrict_mask &= ~RES_KOD;

	/*
	 * We have tossed out as many buggy packets as possible early in
	 * the game to reduce the exposure to a clogging attack. now we
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
	 * Notwithstanding this technobabble, if the sender is a
	 * multicaster, the broadcast address is null, so we use the
	 * unicast address anyway. Don't ask.
	 */
	peer = findpeer(rbufp,  hismode, &retcode);
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
		restrict_mask &= ~RES_MSSNTP;
		is_authentic = AUTH_NONE; /* not required */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d len %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode,
			    authlen);
#endif
	} else if (has_mac == 4) {
		restrict_mask &= ~RES_MSSNTP;
		is_authentic = AUTH_CRYPTO; /* crypto-NAK */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d keyid %08x len %d auth %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode, skeyid,
			    authlen + has_mac, is_authentic);
#endif

#ifdef HAVE_NTP_SIGND
		/*
		 * If the signature is 20 bytes long, the last 16 of
		 * which are zero, then this is a Microsoft client
		 * wanting AD-style authentication of the server's
		 * reply.  
		 *
		 * This is described in Microsoft's WSPP docs, in MS-SNTP:
		 * http://msdn.microsoft.com/en-us/library/cc212930.aspx
		 */
	} else if (has_mac == MAX_MD5_LEN && (restrict_mask & RES_MSSNTP) &&
	   (retcode == AM_FXMIT || retcode == AM_NEWPASS) &&
	   (memcmp(zero_key, (char *)pkt + authlen + 4, MAX_MD5_LEN - 4) ==
	   0)) {
		is_authentic = AUTH_NONE;
#endif /* HAVE_NTP_SIGND */

	} else {
		restrict_mask &= ~RES_MSSNTP;
#ifdef OPENSSL
		/*
		 * For autokey modes, generate the session key
		 * and install in the key cache. Use the socket
		 * broadcast or unicast address as appropriate.
		 */
		if (crypto_flags && skeyid > NTP_MAXKEY) {
		
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
			if (has_mac < MAX_MD5_LEN) {
				sys_badauth++;
				return;
			}
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
				if (!SOCK_UNSPEC(&rbufp->dstadr->bcast))
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
		    has_mac))
			is_authentic = AUTH_ERROR;
		else
			is_authentic = AUTH_OK;
#ifdef OPENSSL
		if (crypto_flags && skeyid > NTP_MAXKEY)
			authtrust(skeyid, 0);
#endif /* OPENSSL */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d keyid %08x len %d auth %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode, skeyid,
			    authlen + has_mac, is_authentic);
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
		 * If authentication OK, send a server reply; otherwise,
		 * send a crypto-NAK.
		 */
		if (!(rbufp->dstadr->flags & INT_MCASTOPEN)) {
			if (AUTH(restrict_mask & RES_DONTTRUST,
			   is_authentic)) {
				fast_xmit(rbufp, MODE_SERVER, skeyid,
				    restrict_mask);
			} else if (is_authentic == AUTH_ERROR) {
				fast_xmit(rbufp, MODE_SERVER, 0,
				    restrict_mask);
				sys_badauth++;
			} else {
				sys_restricted++;
			}
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
		 * Do not respond if we are not synchronized or our
		 * stratum is greater than the manycaster or the
		 * manycaster has already synchronized to us.
		 */
		if (sys_leap == LEAP_NOTINSYNC || sys_stratum >=
		    hisstratum || (!sys_cohort && sys_stratum ==
		    hisstratum + 1) || rbufp->dstadr->addr_refid ==
		    pkt->refid) {
			sys_declined++;
			return;			/* no help */
		}

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
		    (RES_NOPEER | RES_DONTTRUST)), is_authentic)) {
			sys_restricted++;
			return;			/* access denied */
		}

		/*
		 * Do not respond if unsynchronized or stratum is below
		 * the floor or at or above the ceiling.
		 */
		if (hisleap == LEAP_NOTINSYNC || hisstratum <
		    sys_floor || hisstratum >= sys_ceiling) {
			sys_declined++;
			return;			/* no help */
		}
		if ((peer2 = findmanycastpeer(rbufp)) == NULL) {
			sys_restricted++;
			return;			/* not enabled */
		}
		if ((peer = newpeer(&rbufp->recv_srcadr, rbufp->dstadr,
		    MODE_CLIENT, hisversion, NTP_MINDPOLL, NTP_MAXDPOLL,
		    FLAG_PREEMPT, MDF_UCAST | MDF_ACLNT, 0, skeyid)) ==
		    NULL) {
			sys_declined++;
			return;			/* ignore duplicate  */
		}

		/*
		 * We don't need these, but it warms the billboards.
		 */
		if (peer2->flags & FLAG_IBURST)
			peer->flags |= FLAG_IBURST;
		peer->minpoll = peer2->minpoll;
		peer->maxpoll = peer2->maxpoll;
		break;

	/*
	 * This is the first packet received from a broadcast server. If
	 * the packet is authentic and we are enabled as broadcast
	 * client, mobilize a broadcast client association. We don't
	 * kiss any frogs here.
	 */
	case AM_NEWBCL:
		if (sys_bclient == 0) {
			sys_restricted++;
			return;			/* not enabled */
		}
		if (!AUTH(sys_authenticate | (restrict_mask &
		    (RES_NOPEER | RES_DONTTRUST)), is_authentic)) {
			sys_restricted++;
			return;			/* access denied */
		}

		/*
		 * Do not respond if unsynchronized or stratum is below
		 * the floor or at or above the ceiling.
		 */
		if (hisleap == LEAP_NOTINSYNC || hisstratum <
		    sys_floor || hisstratum >= sys_ceiling) {
			sys_declined++;
			return;			/* no help */
		}

#ifdef OPENSSL
		/*
		 * Do not respond if Autokey and the opcode is not a
		 * CRYPTO_ASSOC response with associationn ID.
		 */
		if (crypto_flags && skeyid > NTP_MAXKEY && (opcode &
		    0xffff0000) != (CRYPTO_ASSOC | CRYPTO_RESP)) {
			sys_declined++;
			return;			/* protocol error */
		}
#endif /* OPENSSL */

		/*
		 * Broadcasts received via a multicast address may
		 * arrive after a unicast volley has begun
		 * with the same remote address.  newpeer() will not
		 * find duplicate associations on other local endpoints
		 * if a non-NULL endpoint is supplied.  multicastclient
		 * ephemeral associations are unique across all local
		 * endpoints.
		 */
		if (!(INT_MCASTOPEN & rbufp->dstadr->flags))
			match_ep = rbufp->dstadr;
		else
			match_ep = NULL;

		/*
		 * Determine whether to execute the initial volley.
		 */
		if (sys_bdelay != 0) {
#ifdef OPENSSL
			/*
			 * If a two-way exchange is not possible,
			 * neither is Autokey.
			 */
			if (crypto_flags && skeyid > NTP_MAXKEY) {
				sys_restricted++;
				return;		/* no autokey */
			}
#endif /* OPENSSL */

			/*
			 * Do not execute the volley. Start out in
			 * broadcast client mode.
			 */
			peer = newpeer(&rbufp->recv_srcadr, match_ep,
			    MODE_BCLIENT, hisversion, pkt->ppoll,
			    pkt->ppoll, FLAG_PREEMPT, MDF_BCLNT, 0,
			    skeyid);
			if (NULL == peer) {
				sys_restricted++;
				return;		/* ignore duplicate */

			} else {
				peer->delay = sys_bdelay;
				peer->bias = -sys_bdelay / 2.;
			}
			break;
		}

		/*
		 * Execute the initial volley in order to calibrate the
		 * propagation delay and run the Autokey protocol.
		 *
		 * Note that the minpoll is taken from the broadcast
		 * packet, normally 6 (64 s) and that the poll interval
		 * is fixed at this value.
		 */
		peer = newpeer(&rbufp->recv_srcadr, match_ep,
		    MODE_CLIENT, hisversion, pkt->ppoll, pkt->ppoll,
		    FLAG_BC_VOL | FLAG_IBURST | FLAG_PREEMPT, MDF_BCLNT,
		    0, skeyid);
		if (NULL == peer) {
			sys_restricted++;
			return;			/* ignore duplicate */
		}
#ifdef OPENSSL
		if (skeyid > NTP_MAXKEY)
			crypto_recv(peer, rbufp);
#endif /* OPENSSL */

		return;				/* hooray */

	/*
	 * This is the first packet received from a symmetric active
	 * peer. If the packet is authentic and the first he sent,
	 * mobilize a passive association. If not, kiss the frog.
	 */
	case AM_NEWPASS:
		if (!AUTH(sys_authenticate | (restrict_mask &
		    (RES_NOPEER | RES_DONTTRUST)), is_authentic)) {

			/*
			 * If authenticated but cannot mobilize an
			 * association, send a symmetric passive
			 * response without mobilizing an association.
			 * This is for drat broken Windows clients. See
			 * Microsoft KB 875424 for preferred workaround.
			 */
			if (AUTH(restrict_mask & RES_DONTTRUST,
			    is_authentic)) {
				fast_xmit(rbufp, MODE_PASSIVE, skeyid,
				    restrict_mask);
				return;			/* hooray */
			}
			if (is_authentic == AUTH_ERROR) {
				fast_xmit(rbufp, MODE_ACTIVE, 0,
				    restrict_mask);
				sys_restricted++;
			}
		}

		/*
		 * Do not respond if synchronized and stratum is either
		 * below the floor or at or above the ceiling. Note,
		 * this allows an unsynchronized peer to synchronize to
		 * us. It would be very strange if he did and then was
		 * nipped, but that could only happen if we were
		 * operating at the top end of the range.
		 */
		if (hisleap != LEAP_NOTINSYNC && (hisstratum <
		    sys_floor || hisstratum >= sys_ceiling)) {
			sys_declined++;
			return;			/* no help */
		}

		/*
		 * The message is correctly authenticated and
		 * allowed. Mobiliae a symmetric passive association.
		 */
		if ((peer = newpeer(&rbufp->recv_srcadr,
		    rbufp->dstadr, MODE_PASSIVE, hisversion, pkt->ppoll,
		    NTP_MAXDPOLL, FLAG_PREEMPT, MDF_UCAST, 0,
		    skeyid)) == NULL) {
			sys_declined++;
			return;			/* ignore duplicate */
		}
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
		sys_declined++;
		return;

	/*
	 * For everything else there is the bit bucket.
	 */
	default:
		sys_declined++;
		return;
	}

#ifdef OPENSSL
	/*
	 * If the association is configured for Autokey, the packet must
	 * have a public key ID; if not, the packet must have a
	 * symmetric key ID.
	 */
	if (is_authentic != AUTH_CRYPTO && (((peer->flags &
	    FLAG_SKEY) && skeyid <= NTP_MAXKEY) || (!(peer->flags &
	    FLAG_SKEY) && skeyid > NTP_MAXKEY))) {
		sys_badauth++;
		return;
	}
#endif /* OPENSSL */
	peer->received++;
	peer->flash &= ~PKT_TEST_MASK;
	if (peer->flags & FLAG_XBOGUS) {
		peer->flags &= ~FLAG_XBOGUS;
		peer->flash |= TEST3;
	}

	/*
	 * Next comes a rigorous schedule of timestamp checking. If the
	 * transmit timestamp is zero, the server has not initialized in
	 * interleaved modes or is horribly broken.
	 */
	if (L_ISZERO(&p_xmt)) {
		peer->flash |= TEST3;			/* unsynch */

	/*
	 * If the transmit timestamp duplicates a previous one, the
	 * packet is a replay. This prevents the bad guys from replaying
	 * the most recent packet, authenticated or not.
	 */
	} else if (L_ISEQU(&peer->xmt, &p_xmt)) {
		peer->flash |= TEST1;			/* duplicate */
		peer->oldpkt++;
		return;

	/*
	 * If this is a broadcast mode packet, skip further checking. If
	 * an intial volley, bail out now and let the client do its
	 * stuff. If the origin timestamp is nonzero, this is an
	 * interleaved broadcast. so restart the protocol.
	 */
	} else if (hismode == MODE_BROADCAST) {
		if (!L_ISZERO(&p_org) && !(peer->flags & FLAG_XB)) {
			peer->flags |= FLAG_XB;
			peer->aorg = p_xmt;
			peer->borg = rbufp->recv_time;
			report_event(PEVNT_XLEAVE, peer, NULL);
			return;
		}

	/*
	 * Check for bogus packet in basic mode. If found, switch to
	 * interleaved mode and resynchronize, but only after confirming
	 * the packet is not bogus in symmetric interleaved mode.
	 */
	} else if (peer->flip == 0) {
		if (!L_ISEQU(&p_org, &peer->aorg)) {
			peer->bogusorg++;
			peer->flash |= TEST2;	/* bogus */
			if (!L_ISZERO(&peer->dst) && L_ISEQU(&p_org,
			    &peer->dst)) {
				peer->flip = 1;
				report_event(PEVNT_XLEAVE, peer, NULL);
			}
		} else {
			L_CLR(&peer->aorg);
		}

	/*
	 * Check for valid nonzero timestamp fields.
	 */
	} else if (L_ISZERO(&p_org) || L_ISZERO(&p_rec) ||
	    L_ISZERO(&peer->dst)) {
		peer->flash |= TEST3;		/* unsynch */

	/*
	 * Check for bogus packet in interleaved symmetric mode. This
	 * can happen if a packet is lost, duplicat or crossed. If
	 * found, flip and resynchronize.
	 */
	} else if (!L_ISZERO(&peer->dst) && !L_ISEQU(&p_org,
		    &peer->dst)) {
			peer->bogusorg++;
			peer->flags |= FLAG_XBOGUS;
			peer->flash |= TEST2;		/* bogus */
	}

	/*
	 * Update the state variables.
	 */
	if (peer->flip == 0) {
		if (hismode != MODE_BROADCAST)
			peer->rec = p_xmt;
		peer->dst = rbufp->recv_time;
	}
	peer->xmt = p_xmt;

	/*
	 * If this is a crypto_NAK, the server cannot authenticate a
	 * client packet. The server might have just changed keys. Clear
	 * the association and restart the protocol.
	 */
	if (is_authentic == AUTH_CRYPTO) {
		report_event(PEVNT_AUTH, peer, "crypto_NAK");
		peer->flash |= TEST5;		/* bad auth */
		peer->badauth++;
		if (peer->flags & FLAG_PREEMPT) {
			unpeer(peer);
			return;
		}
#ifdef OPENSSL
		if (peer->crypto)
			peer_clear(peer, "AUTH");
#endif /* OPENSSL */
		return;

	/* 
	 * If the digest fails, the client cannot authenticate a server
	 * reply to a client packet previously sent. The loopback check
	 * is designed to avoid a bait-and-switch attack, which was
	 * possible in past versions. If symmetric modes, return a
	 * crypto-NAK. The peer should restart the protocol.
	 */
	} else if (!AUTH(has_mac || (restrict_mask & RES_DONTTRUST),
	    is_authentic)) {
		report_event(PEVNT_AUTH, peer, "digest");
		peer->flash |= TEST5;		/* bad auth */
		peer->badauth++;
		if (hismode == MODE_ACTIVE || hismode == MODE_PASSIVE)
			fast_xmit(rbufp, MODE_ACTIVE, 0, restrict_mask);
		if (peer->flags & FLAG_PREEMPT) {
			unpeer(peer);
			return;
		}
#ifdef OPENSSL
		if (peer->crypto)
			peer_clear(peer, "AUTH");
#endif /* OPENSSL */
		return;
	}

	/*
	 * Set the peer ppoll to the maximum of the packet ppoll and the
	 * peer minpoll. If a kiss-o'-death, set the peer minpoll to
	 * this maximumn and advance the headway to give the sender some
	 * headroom. Very intricate.
	 */
	peer->ppoll = max(peer->minpoll, pkt->ppoll);
	if (hismode == MODE_SERVER && hisleap == LEAP_NOTINSYNC &&
	    hisstratum == STRATUM_UNSPEC && memcmp(&pkt->refid,
	    "RATE", 4) == 0) {
		peer->selbroken++;
		report_event(PEVNT_RATE, peer, NULL);
		if (pkt->ppoll > peer->minpoll)
			peer->minpoll = peer->ppoll;
		peer->burst = peer->retry = 0;
		peer->throttle = (NTP_SHIFT + 1) * (1 << peer->minpoll);
		poll_update(peer, pkt->ppoll);
		return;				/* kiss-o'-death */
	}

	/*
	 * That was hard and I am sweaty, but the packet is squeaky
	 * clean. Get on with real work.
	 */
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
	 *    match, sit the dance and call for new autokey values.
	 *
	 * In case of crypto error, fire the orchestra, stop dancing and
	 * restart the protocol.
	 */
	if (peer->flags & FLAG_SKEY) {
		/*
		 * Decrement remaining audokey hashes. This isn't
		 * perfect if a packet is lost, but results in no harm.
		 */
		ap = (struct autokey *)peer->recval.ptr;
		if (ap != NULL) {
			if (ap->seq > 0)
				ap->seq--;
		}
		peer->flash |= TEST8;
		rval = crypto_recv(peer, rbufp);
		if (rval == XEVNT_OK) {
			peer->unreach = 0;
		} else {
			if (rval == XEVNT_ERR) {
				report_event(PEVNT_RESTART, peer,
				    "crypto error");
				peer_clear(peer, "CRYP");
				peer->flash |= TEST9;	/* bad crypt */
				if (peer->flags & FLAG_PREEMPT)
					unpeer(peer);
			}
			return;
		}

		/*
		 * If server mode, verify the receive key ID matches
		 * the transmit key ID.
		 */
		if (hismode == MODE_SERVER) {
			if (skeyid == peer->keyid)
				peer->flash &= ~TEST8;

		/*
		 * If an extension field is present, verify only that it
		 * has been correctly signed. We don't need a sequence
		 * check here, but the sequence continues.
		 */
		} else if (!(peer->flash & TEST8)) {
			peer->pkeyid = skeyid;

		/*
		 * Now the fun part. Here, skeyid is the current ID in
		 * the packet, pkeyid is the ID in the last packet and
		 * tkeyid is the hash of skeyid. If the autokey values
		 * have not been received, this is an automatic error.
		 * If so, check that the tkeyid matches pkeyid. If not,
		 * hash tkeyid and try again. If the number of hashes
		 * exceeds the number remaining in the sequence, declare
		 * a successful failure and refresh the autokey values.
		 */
		} else if (ap != NULL) {
			int i;

			for (i = 0; ; i++) {
				if (tkeyid == peer->pkeyid ||
				    tkeyid == ap->key) {
					peer->flash &= ~TEST8;
					peer->pkeyid = skeyid;
					ap->seq -= i;
					break;
				}
				if (i > ap->seq) {
					peer->crypto &=
					    ~CRYPTO_FLAG_AUTO;
					break;
				}
				tkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin,
				    tkeyid, pkeyid, 0);
			}
			if (peer->flash & TEST8)
				report_event(PEVNT_AUTH, peer, "keylist");
		}
		if (!(peer->crypto & CRYPTO_FLAG_PROV)) /* test 9 */
			peer->flash |= TEST8;	/* bad autokey */

		/*
		 * The maximum lifetime of the protocol is about one
		 * week before restarting the Autokey protocol to
		 * refreshed certificates and leapseconds values.
		 */
		if (current_time > peer->refresh) {
			report_event(PEVNT_RESTART, peer,
			    "crypto refresh");
			peer_clear(peer, "TIME");
			return;
		}
	}
#endif /* OPENSSL */

	/*
	 * The dance is complete and the flash bits have been lit. Toss
	 * the packet over the fence for processing, which may light up
	 * more flashers.
	 */
	process_packet(peer, pkt, rbufp->recv_length);

	/*
	 * In interleaved mode update the state variables. Also adjust the
	 * transmit phase to avoid crossover.
	 */
	if (peer->flip != 0) {
		peer->rec = p_rec;
		peer->dst = rbufp->recv_time;
		if (peer->nextdate - current_time < (1 << min(peer->ppoll,
		    peer->hpoll)) / 2)
			peer->nextdate++;
		else
			peer->nextdate--;
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
	register struct pkt *pkt,
	u_int	len
	)
{
	double	t34, t21;
	double	p_offset, p_del, p_disp;
	l_fp	p_rec, p_xmt, p_org, p_reftime, ci;
	u_char	pmode, pleap, pstratum;
	char	statstr[NTP_MAXSTRLEN];
#ifdef ASSYM
	int	itemp;
	double	etemp, ftemp, td;
#endif /* ASSYM */

	sys_processed++;
	peer->processed++;
	p_del = FPTOD(NTOHS_FP(pkt->rootdelay));
	p_offset = 0;
	p_disp = FPTOD(NTOHS_FP(pkt->rootdisp));
	NTOHL_FP(&pkt->reftime, &p_reftime);
	NTOHL_FP(&pkt->org, &p_org);
	NTOHL_FP(&pkt->rec, &p_rec);
	NTOHL_FP(&pkt->xmt, &p_xmt);
	pmode = PKT_MODE(pkt->li_vn_mode);
	pleap = PKT_LEAP(pkt->li_vn_mode);
	pstratum = PKT_TO_STRATUM(pkt->stratum);

	/*
	 * Capture the header values in the client/peer association..
	 */
	record_raw_stats(&peer->srcadr, peer->dstadr ?
	    &peer->dstadr->sin : NULL, &p_org, &p_rec, &p_xmt,
	    &peer->dst);
	peer->leap = pleap;
	peer->stratum = min(pstratum, STRATUM_UNSPEC);
	peer->pmode = pmode;
	peer->precision = pkt->precision;
	peer->rootdelay = p_del;
	peer->rootdisp = p_disp;
	peer->refid = pkt->refid;		/* network byte order */
	peer->reftime = p_reftime;

	/*
	 * First, if either burst mode is armed, enable the burst.
	 * Compute the headway for the next packet and delay if
	 * necessary to avoid exceeding the threshold.
	 */
	if (peer->retry > 0) {
		peer->retry = 0;
		if (peer->reach)
			peer->burst = min(1 << (peer->hpoll -
			    peer->minpoll), NTP_SHIFT) - 1;
		else
			peer->burst = NTP_IBURST - 1;
		if (peer->burst > 0)
			peer->nextdate = current_time;
	}
	poll_update(peer, peer->hpoll);

	/*
	 * Verify the server is synchronized; that is, the leap bits,
	 * stratum and root distance are valid.
	 */
	if (pleap == LEAP_NOTINSYNC ||		/* test 6 */
	    pstratum < sys_floor || pstratum >= sys_ceiling)
		peer->flash |= TEST6;		/* bad synch or strat */
	if (p_del / 2 + p_disp >= MAXDISPERSE)	/* test 7 */
		peer->flash |= TEST7;		/* bad header */

	/*
	 * If any tests fail at this point, the packet is discarded.
	 * Note that some flashers may have already been set in the
	 * receive() routine.
	 */
	if (peer->flash & PKT_TEST_MASK) {
		peer->seldisptoolarge++;
#ifdef DEBUG
		if (debug)
			printf("packet: flash header %04x\n",
			    peer->flash);
#endif
		return;
	}

	/*
	 * If the peer was previously unreachable, raise a trap. In any
	 * case, mark it reachable.
	 */ 
	if (!peer->reach) {
		report_event(PEVNT_REACH, peer, NULL);
		peer->timereachable = current_time;
	}
	peer->reach |= 1;

	/*
	 * For a client/server association, calculate the clock offset,
	 * roundtrip delay and dispersion. The equations are reordered
	 * from the spec for more efficient use of temporaries. For a
	 * broadcast association, offset the last measurement by the
	 * computed delay during the client/server volley. Note the
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
	 * double arithmetic. This preserves the accuracy while
	 * retaining the 68-year span.
	 *
	 * There are three interleaving schemes, basic, interleaved
	 * symmetric and interleaved broadcast. The timestamps are
	 * idioscyncratically different. See the onwire briefing/white
	 * paper at www.eecis.udel.edu/~mills for details.
	 *
	 * Interleaved symmetric mode
	 * t1 = peer->aorg/borg, t2 = peer->rec, t3 = p_xmt,
	 * t4 = peer->dst
	 */
	if (peer->flip != 0) {
		ci = p_xmt;				/* t3 - t4 */
		L_SUB(&ci, &peer->dst);
		LFPTOD(&ci, t34);
		ci = p_rec;				/* t2 - t1 */
		if (peer->flip > 0)
			L_SUB(&ci, &peer->borg);
		else
			L_SUB(&ci, &peer->aorg);
		LFPTOD(&ci, t21);
		p_del = t21 - t34;
		p_offset = (t21 + t34) / 2.;
		if (p_del < 0 || p_del > 1.) {
			sprintf(statstr, "t21 %.6f t34 %.6f", t21, t34);
			report_event(PEVNT_XERR, peer, statstr);
			return;
		}

	/*
	 * Broadcast modes
	 */
	} else if (peer->pmode == MODE_BROADCAST) {

		/*
		 * Interleaved broadcast mode. Use interleaved timestamps.
		 * t1 = peer->borg, t2 = p_org, t3 = p_org, t4 = aorg
		 */
		if (peer->flags & FLAG_XB) { 
			ci = p_org;			/* delay */ 
			L_SUB(&ci, &peer->aorg);
			LFPTOD(&ci, t34);
			ci = p_org;			/* t2 - t1 */
			L_SUB(&ci, &peer->borg);
			LFPTOD(&ci, t21);
			peer->aorg = p_xmt;
			peer->borg = peer->dst;
			if (t34 < 0 || t34 > 1.) {
				sprintf(statstr,
				    "offset %.6f delay %.6f", t21, t34);
				report_event(PEVNT_XERR, peer, statstr);
				return;
			}
			p_offset = t21;
			peer->xleave = t34;

		/*
		 * Basic broadcast - use direct timestamps.
		 * t3 = p_xmt, t4 = peer->dst
		 */
		} else {
			ci = p_xmt;		/* t3 - t4 */
			L_SUB(&ci, &peer->dst);
			LFPTOD(&ci, t34);
			p_offset = t34;
		}

		/*
		 * When calibration is complete and the clock is
		 * synchronized, the bias is calculated as the difference
		 * between the unicast timestamp and the broadcast
		 * timestamp. This works for both basic and interleaved
		 * modes.
		 */
		if (FLAG_BC_VOL & peer->flags) {
			peer->flags &= ~FLAG_BC_VOL;
			peer->delay = (peer->offset - p_offset) * 2;
		}
		p_del = peer->delay;
		p_offset += p_del / 2;


	/*
	 * Basic mode, otherwise known as the old fashioned way.
	 *
	 * t1 = p_org, t2 = p_rec, t3 = p_xmt, t4 = peer->dst
	 */
	} else {
		ci = p_xmt;				/* t3 - t4 */
		L_SUB(&ci, &peer->dst);
		LFPTOD(&ci, t34);
		ci = p_rec;				/* t2 - t1 */
		L_SUB(&ci, &p_org);
		LFPTOD(&ci, t21);
		p_del = fabs(t21 - t34);
		p_offset = (t21 + t34) / 2.;
	}
	p_offset += peer->bias;
	p_disp = LOGTOD(sys_precision) + LOGTOD(peer->precision) +
	    clock_phi * p_del;

#if ASSYM
	/*
	 * This code calculates the outbound and inbound data rates by
	 * measuring the differences between timestamps at different
	 * packet lengths. This is helpful in cases of large asymmetric
	 * delays commonly experienced on deep space communication
	 * links.
	 */
	if (peer->t21_last > 0 && peer->t34_bytes > 0) {
		itemp = peer->t21_bytes - peer->t21_last;
		if (itemp > 25) {
			etemp = t21 - peer->t21;
			if (fabs(etemp) > 1e-6) {
				ftemp = itemp / etemp;
				if (ftemp > 1000.)
					peer->r21 = ftemp;
			}
		}
		itemp = len - peer->t34_bytes;
		if (itemp > 25) {
			etemp = -t34 - peer->t34;
			if (fabs(etemp) > 1e-6) {
				ftemp = itemp / etemp;
				if (ftemp > 1000.)
					peer->r34 = ftemp;
			}
		}
	}

	/*
	 * The following section compensates for different data rates on
	 * the outbound (d21) and inbound (t34) directions. To do this,
	 * it finds t such that r21 * t - r34 * (d - t) = 0, where d is
	 * the roundtrip delay. Then it calculates the correction as a
	 * fraction of d.
	 */
 	peer->t21 = t21;
	peer->t21_last = peer->t21_bytes;
	peer->t34 = -t34;
	peer->t34_bytes = len;
#ifdef DEBUG
	if (debug > 1)
		printf("packet: t21 %.9lf %d t34 %.9lf %d\n", peer->t21,
		    peer->t21_bytes, peer->t34, peer->t34_bytes);
#endif
	if (peer->r21 > 0 && peer->r34 > 0 && p_del > 0) {
		if (peer->pmode != MODE_BROADCAST)
			td = (peer->r34 / (peer->r21 + peer->r34) -
			    .5) * p_del;
		else
			td = 0;

		/*
 		 * Unfortunately, in many cases the errors are
		 * unacceptable, so for the present the rates are not
		 * used. In future, we might find conditions where the
		 * calculations are useful, so this should be considered
		 * a work in progress.
		 */
		t21 -= td;
		t34 -= td;
#ifdef DEBUG
		if (debug > 1)
			printf("packet: del %.6lf r21 %.1lf r34 %.1lf %.6lf\n",
			    p_del, peer->r21 / 1e3, peer->r34 / 1e3,
			    td);
#endif
	} 
#endif /* ASSYM */

	/*
	 * That was awesome. Now hand off to the clock filter.
	 */
	clock_filter(peer, p_offset, p_del, p_disp);

	/*
	 * If we are in broadcast calibrate mode, return to broadcast
	 * client mode when the client is fit and the autokey dance is
	 * complete.
	 */
	if ((FLAG_BC_VOL & peer->flags) && MODE_CLIENT == peer->hmode &&
	    !(TEST11 & peer_unfit(peer))) {	/* distance exceeded */
#ifdef OPENSSL
		if (peer->flags & FLAG_SKEY) {
			if (!(~peer->crypto & CRYPTO_FLAG_ALL))
				peer->hmode = MODE_BCLIENT;
		} else {
			peer->hmode = MODE_BCLIENT;
		}
#else /* OPENSSL */
		peer->hmode = MODE_BCLIENT;
#endif /* OPENSSL */
	}
}


/*
 * clock_update - Called at system process update intervals.
 */
static void
clock_update(
	struct peer *peer	/* peer structure pointer */
	)
{
	double	dtemp;
	l_fp	now;
#ifdef HAVE_LIBSCF_H
	char	*fmri;
#endif /* HAVE_LIBSCF_H */

	/*
	 * Update the system state variables. We do this very carefully,
	 * as the poll interval might need to be clamped differently.
	 */
	sys_peer = peer;
	sys_epoch = peer->epoch;
	if (sys_poll < peer->minpoll)
		sys_poll = peer->minpoll;
	if (sys_poll > peer->maxpoll)
		sys_poll = peer->maxpoll;
	poll_update(peer, sys_poll);
	sys_stratum = min(peer->stratum + 1, STRATUM_UNSPEC);
	if (peer->stratum == STRATUM_REFCLOCK ||
	    peer->stratum == STRATUM_UNSPEC)
		sys_refid = peer->refid;
	else
		sys_refid = addr2refid(&peer->srcadr);
	dtemp = sys_jitter + fabs(sys_offset) + peer->disp +
	    (peer->delay + peer->rootdelay) / 2 + clock_phi *
	    (current_time - peer->update);
	sys_rootdisp = dtemp + peer->rootdisp;
	sys_rootdelay = peer->delay + peer->rootdelay;
	sys_reftime = peer->dst;

#ifdef DEBUG
	if (debug)
		printf(
		    "clock_update: at %lu sample %lu associd %d\n",
		    current_time, peer->epoch, peer->associd);
#endif

	/*
	 * Comes now the moment of truth. Crank the clock discipline and
	 * see what comes out.
	 */
	switch (local_clock(peer, sys_offset)) {

	/*
	 * Clock exceeds panic threshold. Life as we know it ends.
	 */
	case -1:
#ifdef HAVE_LIBSCF_H
		/*
		 * For Solaris enter the maintenance mode.
		 */
		if ((fmri = getenv("SMF_FMRI")) != NULL) {
			if (smf_maintain_instance(fmri, 0) < 0) {
				printf("smf_maintain_instance: %s\n",
				    scf_strerror(scf_error()));
				exit(1);
			}
			/*
			 * Sleep until SMF kills us.
			 */
			for (;;)
				pause();
		}
#endif /* HAVE_LIBSCF_H */
		exit (-1);
		/* not reached */

	/*
	 * Clock was stepped. Flush all time values of all peers.
	 */
	case 2:
		clear_all();
		sys_leap = LEAP_NOTINSYNC;
		sys_stratum = STRATUM_UNSPEC;
		memcpy(&sys_refid, "STEP", 4);
		sys_rootdelay = 0;
		sys_rootdisp = 0;
		L_CLR(&sys_reftime);
		sys_jitter = LOGTOD(sys_precision);
		leapsec = 0;
		break;

	/*
	 * Clock was slewed. Handle the leapsecond stuff.
	 */
	case 1:

		/*
		 * If this is the first time the clock is set, reset the
		 * leap bits. If crypto, the timer will goose the setup
		 * process.
		 */
		if (sys_leap == LEAP_NOTINSYNC) {
			sys_leap = LEAP_NOWARNING;
#ifdef OPENSSL
			if (crypto_flags)
				crypto_update();
#endif /* OPENSSL */
		}

		/*
		 * If the leapseconds values are from file or network
		 * and the leap is in the future, schedule a leap at the
		 * given epoch. Otherwise, if the number of survivor
		 * leap bits is greater than half the number of
		 * survivors, schedule a leap for the end of the current
		 * month.
		 */
		get_systime(&now);
		if (leap_sec > 0) {
			if (leap_sec > now.l_ui) {
				sys_tai = leap_tai - 1;
				if (leapsec == 0)
					report_event(EVNT_ARMED, NULL,
					    NULL);
				leapsec = leap_sec - now.l_ui;
			} else {
				sys_tai = leap_tai;
			}
			break;

		} else if (leap_vote > sys_survivors / 2) {
			leap_peers = now.l_ui + leap_month(now.l_ui);
			if (leap_peers > now.l_ui) {
				if (leapsec == 0)
					report_event(PEVNT_ARMED, peer,
					    NULL);
				leapsec = leap_peers - now.l_ui;
			}
		} else if (leapsec > 0) {
			report_event(EVNT_DISARMED, NULL, NULL);
			leapsec = 0;
		}
		break;

	/*
	 * Popcorn spike or step threshold exceeded. Pretend it never
	 * happened.
	 */
	default:
		break;
	}
}


/*
 * poll_update - update peer poll interval
 */
void
poll_update(
	struct peer *peer,	/* peer structure pointer */
	int	mpoll
	)
{
	int	hpoll, minpkt;
	u_long	next, utemp;

	/*
	 * This routine figures out when the next poll should be sent.
	 * That turns out to be wickedly complicated. One problem is
	 * that sometimes the time for the next poll is in the past when
	 * the poll interval is reduced. We watch out for races here
	 * between the receive process and the poll process.
	 *
	 * First, bracket the poll interval according to the type of
	 * association and options. If a fixed interval is configured,
	 * use minpoll. This primarily is for reference clocks, but
	 * works for any association. Otherwise, clamp the poll interval
	 * between minpoll and maxpoll.
	 */
	if (peer->cast_flags & MDF_BCLNT)
		hpoll = peer->minpoll;
	else
		hpoll = max(min(peer->maxpoll, mpoll), peer->minpoll);

#ifdef OPENSSL
	/*
	 * If during the crypto protocol the poll interval has changed,
	 * the lifetimes in the key list are probably bogus. Purge the
	 * the key list and regenerate it later.
	 */
	if ((peer->flags & FLAG_SKEY) && hpoll != peer->hpoll)
		key_expire(peer);
#endif /* OPENSSL */
	peer->hpoll = hpoll;

	/*
	 * There are three variables important for poll scheduling, the
	 * current time (current_time), next scheduled time (nextdate)
	 * and the earliest time (utemp). The earliest time is 2 s
	 * seconds, but could be more due to rate management. When
	 * sending in a burst, use the earliest time. When not in a
	 * burst but with a reply pending, send at the earliest time
	 * unless the next scheduled time has not advanced. This can
	 * only happen if multiple replies are peinding in the same
	 * response interval. Otherwise, send at the later of the next
	 * scheduled time and the earliest time.
	 *
	 * Now we figure out if there is an override. If a burst is in
	 * progress and we get called from the receive process, just
	 * slink away. If called from the poll process, delay 1 s for a
	 * reference clock, otherwise 2 s.
	 */
	minpkt = 1 << ntp_minpkt;
	utemp = current_time + max(peer->throttle - (NTP_SHIFT - 1) *
	    (1 << peer->minpoll), minpkt);
	if (peer->burst > 0) {
		if (peer->nextdate > current_time)
			return;
#ifdef REFCLOCK
		else if (peer->flags & FLAG_REFCLOCK)
			peer->nextdate = current_time + RESP_DELAY;
#endif /* REFCLOCK */
		else
			peer->nextdate = utemp;

#ifdef OPENSSL
	/*
	 * If a burst is not in progress and a crypto response message
	 * is pending, delay 2 s, but only if this is a new interval.
	 */
	} else if (peer->cmmd != NULL) {
		if (peer->nextdate > current_time) {
			if (peer->nextdate + minpkt != utemp)
				peer->nextdate = utemp;
		} else {
			peer->nextdate = utemp;
		}
#endif /* OPENSSL */

	/*
	 * The ordinary case. If a retry, use minpoll; if unreachable,
	 * use host poll; otherwise, use the minimum of host and peer
	 * polls; In other words, oversampling is okay but
	 * understampling is evil. Use the maximum of this value and the
	 * headway. If the average headway is greater than the headway
	 * threshold, increase the headway by the minimum interval.
	 */
	} else {
		if (peer->retry > 0)
			hpoll = peer->minpoll;
		else if (!(peer->reach))
			hpoll = peer->hpoll;
		else
			hpoll = min(peer->ppoll, peer->hpoll);
#ifdef REFCLOCK
		if (peer->flags & FLAG_REFCLOCK)
			next = 1 << hpoll;
		else
			next = ((0x1000UL | (ntp_random() & 0x0ff)) <<
			    hpoll) >> 12;
#else /* REFCLOCK */
		next = ((0x1000UL | (ntp_random() & 0x0ff)) << hpoll) >>
		    12;
#endif /* REFCLOCK */
		next += peer->outdate;
		if (next > utemp)
			peer->nextdate = next;
		else
			peer->nextdate = utemp;
		hpoll = peer->throttle - (1 << peer->minpoll);
		if (hpoll > 0)
			peer->nextdate += minpkt;
	}
#ifdef DEBUG
	if (debug > 1)
		printf("poll_update: at %lu %s poll %d burst %d retry %d head %d early %lu next %lu\n",
		    current_time, ntoa(&peer->srcadr), peer->hpoll,
		    peer->burst, peer->retry, peer->throttle,
		    utemp - current_time, peer->nextdate -
		    current_time);
#endif
}


/*
 * peer_clear - clear peer filter registers.  See Section 3.4.8 of the
 * spec.
 */
void
peer_clear(
	struct peer *peer,		/* peer structure */
	char	*ident			/* tally lights */
	)
{
	int	i;

#ifdef OPENSSL
	/*
	 * If cryptographic credentials have been acquired, toss them to
	 * Valhalla. Note that autokeys are ephemeral, in that they are
	 * tossed immediately upon use. Therefore, the keylist can be
	 * purged anytime without needing to preserve random keys. Note
	 * that, if the peer is purged, the cryptographic variables are
	 * purged, too. This makes it much harder to sneak in some
	 * unauthenticated data in the clock filter.
	 */
	key_expire(peer);
	if (peer->iffval != NULL)
		BN_free(peer->iffval);
	value_free(&peer->cookval);
	value_free(&peer->recval);
	value_free(&peer->encrypt);
	value_free(&peer->sndval);
	if (peer->cmmd != NULL)
		free(peer->cmmd);
	if (peer->subject != NULL)
		free(peer->subject);
	if (peer->issuer != NULL)
		free(peer->issuer);
#endif /* OPENSSL */

	/*
	 * Clear all values, including the optional crypto values above.
	 */
	memset(CLEAR_TO_ZERO(peer), 0, LEN_CLEAR_TO_ZERO);
	peer->ppoll = peer->maxpoll;
	peer->hpoll = peer->minpoll;
	peer->disp = MAXDISPERSE;
	peer->flash = peer_unfit(peer);
	peer->jitter = LOGTOD(sys_precision);

	/*
	 * If interleave mode, initialize the alternate origin switch.
	 */
	if (peer->flags & FLAG_XLEAVE)
		peer->flip = 1;
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
	 * the polls at one-second intervals. Otherwise, randomize over
	 * the minimum poll interval in order to avoid broadcast
	 * implosion.
	 */
	peer->nextdate = peer->update = peer->outdate = current_time;
	if (initializing) {
		peer->nextdate += peer_associations;
	} else if (peer->hmode == MODE_PASSIVE) {
		peer->nextdate += 1 << ntp_minpkt;
	} else {
		peer->nextdate += ntp_random() % peer_associations;
	}
#ifdef OPENSSL
	peer->refresh = current_time + (1 << NTP_REFRESH);
#endif /* OPENSSL */
#ifdef DEBUG
	if (debug)
		printf(
		    "peer_clear: at %ld next %ld associd %d refid %s\n",
		    current_time, peer->nextdate, peer->associd,
		    ident);
#endif
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
	char	tbuf[80];

	/*
	 * A sample consists of the offset, delay, dispersion and epoch
	 * of arrival. The offset and delay are determined by the on-
	 * wire protocol. The dispersion grows from the last outbound
	 * packet to the arrival of this one increased by the sum of the
	 * peer precision and the system precision as required by the
	 * error budget. First, shift the new arrival into the shift
	 * register discarding the oldest one.
	 */
	j = peer->filter_nextpt;
	peer->filter_offset[j] = sample_offset;
	peer->filter_delay[j] = sample_delay;
	peer->filter_disp[j] = sample_disp;
	peer->filter_epoch[j] = current_time;
	j = (j + 1) % NTP_SHIFT;
	peer->filter_nextpt = j;

	/*
	 * Update dispersions since the last update and at the same
	 * time initialize the distance and index lists. Since samples
	 * become increasingly uncorrelated beyond the Allan intercept,
	 * only under exceptional cases will an older sample be used.
	 * Therefore, the distance list uses a compound metric. If the
	 * dispersion is greater than the maximum dispersion, clamp the
	 * distance at that value. If the time since the last update is
	 * less than the Allan intercept use the delay; otherwise, use
	 * the sum of the delay and dispersion.
	 */
	dtemp = clock_phi * (current_time - peer->update);
	peer->update = current_time;
	for (i = NTP_SHIFT - 1; i >= 0; i--) {
		if (i != 0)
			peer->filter_disp[j] += dtemp;
		if (peer->filter_disp[j] >= MAXDISPERSE) { 
			peer->filter_disp[j] = MAXDISPERSE;
			dst[i] = MAXDISPERSE;
		} else if (peer->update - peer->filter_epoch[j] >
		    ULOGTOD(allan_xpt)) {
			dst[i] = peer->filter_delay[j] +
			    peer->filter_disp[j];
		} else {
			dst[i] = peer->filter_delay[j];
		}
		ord[i] = j;
		j = (j + 1) % NTP_SHIFT;
	}

        /*
	 * If the clock discipline has stabilized, sort the samples by
	 * distance.  
	 */
	if (sys_leap != LEAP_NOTINSYNC) {
		for (i = 1; i < NTP_SHIFT; i++) {
			for (j = 0; j < i; j++) {
				if (dst[j] > dst[i]) {
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
	 * can see it later. Prune the distance list to leave only
	 * samples less than the maximum dispersion, which disfavors
	 * uncorrelated samples older than the Allan intercept. To
	 * further improve the jitter estimate, of the remainder leave
	 * only samples less than the maximum distance, but keep at
	 * least two samples for jitter calculation.
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
	 * lowest delay sample.
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
	if (m == 0) {
		clock_select();
		return;
	}

	etemp = fabs(peer->offset - peer->filter_offset[k]);
	peer->offset = peer->filter_offset[k];
	peer->delay = peer->filter_delay[k];
	if (m > 1)
		peer->jitter /= m - 1;
	peer->jitter = max(SQRT(peer->jitter), LOGTOD(sys_precision));

	/*
	 * If the the new sample and the current sample are both valid
	 * and the difference between their offsets exceeds CLOCK_SGATE
	 * (3) times the jitter and the interval between them is less
	 * than twice the host poll interval, consider the new sample
	 * a popcorn spike and ignore it.
	 */
	if (peer->disp < sys_maxdist && peer->filter_disp[k] <
	    sys_maxdist && etemp > CLOCK_SGATE * peer->jitter &&
	    peer->filter_epoch[k] - peer->epoch < 2. *
	    ULOGTOD(peer->hpoll)) {
		snprintf(tbuf, sizeof(tbuf), "%.6f s", etemp);
		report_event(PEVNT_POPCORN, peer, tbuf);
		return;
	}

	/*
	 * A new minimum sample is useful only if it is later than the
	 * last one used. In this design the maximum lifetime of any
	 * sample is not greater than eight times the poll interval, so
	 * the maximum interval between minimum samples is eight
	 * packets.
	 */
	if (peer->filter_epoch[k] <= peer->epoch) {
#if DEBUG
	if (debug)
		printf("clock_filter: old sample %lu\n", current_time -
		    peer->filter_epoch[k]);
#endif
		return;
	}
	peer->epoch = peer->filter_epoch[k];

	/*
	 * The mitigated sample statistics are saved for later
	 * processing. If not synchronized or not in a burst, tickle the
	 * clock select algorithm.
	 */
	record_peer_stats(&peer->srcadr, ctlpeerstatus(peer),
	    peer->offset, peer->delay, peer->disp, peer->jitter);
#ifdef DEBUG
	if (debug)
		printf(
		    "clock_filter: n %d off %.6f del %.6f dsp %.6f jit %.6f\n",
		    m, peer->offset, peer->delay, peer->disp,
		    peer->jitter);
#endif
	if (peer->burst == 0 || sys_leap == LEAP_NOTINSYNC)
		clock_select();
}


/*
 * clock_select - find the pick-of-the-litter clock
 *
 * LOCKCLOCK: (1) If the local clock is the prefer peer, it will always
 * be enabled, even if declared falseticker, (2) only the prefer peer
 * caN Be selected as the system peer, (3) if the external source is
 * down, the system leap bits are set to 11 and the stratum set to
 * infinity.
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
	double	seljitter;
	double	synch[NTP_MAXASSOC], error[NTP_MAXASSOC];
	double	orphmet = 2.0 * U_INT32_MAX; /* 2x is greater than */
	struct peer *osys_peer = NULL;
	struct peer *sys_prefer = NULL;	/* prefer peer */
	struct peer *typesystem = NULL;
	struct peer *typeorphan = NULL;
#ifdef REFCLOCK
	struct peer *typeacts = NULL;
	struct peer *typelocal = NULL;
	struct peer *typepps = NULL;
#endif /* REFCLOCK */

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
			peer->new_status = CTL_PST_SEL_REJECT;

			/*
			 * Leave the island immediately if the peer is
			 * unfit to synchronize.
			 */
			if (peer_unfit(peer))
				continue;

			/*
			 * If this peer is an orphan parent, elect the
			 * one with the lowest metric defined as the
			 * IPv4 address or the first 64 bits of the
			 * hashed IPv6 address.  To ensure convergence
			 * on the same selected orphan, consider as
			 * well that this system may have the lowest
			 * metric and be the orphan parent.  If this
			 * system wins, sys_peer will be NULL to trigger
			 * orphan mode in timer().
			 */
			if (peer->stratum == sys_orphan) {
				u_int32	localmet;
				u_int32	peermet;

				if (peer->dstadr != NULL)
					localmet = ntohl(peer->dstadr->addr_refid);
				else
					localmet = U_INT32_MAX;
				peermet = ntohl(addr2refid(&peer->srcadr));
				if (peermet < localmet &&
				    peermet < orphmet) {
					typeorphan = peer;
					orphmet = peermet;
				}
				continue;
			}

			/*
			 * If this peer could have the orphan parent
			 * as a synchronization ancestor, exclude it
			 * from selection to avoid forming a 
			 * synchronization loop within the orphan mesh,
			 * triggering stratum climb to infinity 
			 * instability.  Peers at stratum higher than
			 * the orphan stratum could have the orphan
			 * parent in ancestry so are excluded.
			 * See http://bugs.ntp.org/2050
			 */
			if (peer->stratum > sys_orphan)
				continue;
#ifdef REFCLOCK
			/*
			 * The following are special cases. We deal
			 * with them later.
			 */
			if (!(peer->flags & FLAG_PREFER)) {
				switch (peer->refclktype) {
				case REFCLK_LOCALCLOCK:
					if (typelocal == NULL)
						typelocal = peer;
					continue;

				case REFCLK_ACTS:
					if (typeacts == NULL)
						typeacts = peer;
					continue;
				}
			}
#endif /* REFCLOCK */

			/*
			 * If we get this far, the peer can stay on the
			 * island, but does not yet have the immunity
			 * idol.
			 */
			peer->new_status = CTL_PST_SEL_SANE;
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
	 * survivors with offsets not less than low and not greater than
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
	 * million bucks.
	 */
	j = 0;
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		if (nlist > 1 && (peer->offset <= low || peer->offset >=
		    high) && !(peer->flags & FLAG_TRUE))
			continue;

#ifdef REFCLOCK
		/*
		 * Eligible PPS peers must survive the intersection
		 * algorithm. Use the first one found, but don't
		 * include any of them in the cluster population.
		 */
		if (peer->flags & FLAG_PPS) {
			if (typepps == NULL) 
				typepps = peer;
			continue;
		}
#endif /* REFCLOCK */

		/*
		 * The metric is the scaled root distance at the next
		 * poll interval plus the peer stratum.
		 */
		d = (root_distance(peer) + clock_phi * (peer->nextdate -
		    current_time)) / sys_maxdist + peer->stratum;
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
	 * If no survivors remain at this point, check if the modem 
	 * driver, local driver or orphan parent in that order. If so,
	 * nominate the first one found as the only survivor.
	 * Otherwise, give up and leave the island to the rats.
	 */
	if (nlist == 0) {
		error[0] = 0;
		synch[0] = 0;
#ifdef REFCLOCK
		if (typeacts != NULL) {
			peer_list[0] = typeacts;
			nlist = 1;
		} else if (typelocal != NULL) {
			peer_list[0] = typelocal;
			nlist = 1;
		} else
#endif /* REFCLOCK */
		if (typeorphan != NULL) {
			peer_list[0] = typeorphan;
			nlist = 1;
		}
	}

	/*
	 * Mark the candidates at this point as truechimers.
	 */
	for (i = 0; i < nlist; i++) {
		peer_list[i]->new_status = CTL_PST_SEL_SELCAND;
#ifdef DEBUG
		if (debug > 1)
			printf("select: survivor %s %f\n",
			    stoa(&peer_list[i]->srcadr), synch[i]);
#endif
	}

	/*
	 * Now, vote outlyers off the island by select jitter weighted
	 * by root distance. Continue voting as long as there are more
	 * than sys_minclock survivors and the minimum select jitter is
	 * greater than the maximum peer jitter. Stop if we are about to
	 * discard a TRUE or PREFER  peer, who of course has the
	 * immunity idol.
	 */
	seljitter = 0;
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
		if (nlist <= sys_minsane || nlist <= sys_minclock) {
			break;

		} else if (f <= d || peer_list[k]->flags &
		    (FLAG_TRUE | FLAG_PREFER)) {
			seljitter = f;
			break;
		}
#ifdef DEBUG
		if (debug > 2)
			printf(
			    "select: drop %s seljit %.6f jit %.6f\n",
			    ntoa(&peer_list[k]->srcadr), g, d);
#endif
		if (nlist > sys_maxclock)
			peer_list[k]->new_status = CTL_PST_SEL_EXCESS;
		for (j = k + 1; j < nlist; j++) {
			peer_list[j - 1] = peer_list[j];
			synch[j - 1] = synch[j];
			error[j - 1] = error[j];
		}
		nlist--;
	}

	/*
	 * What remains is a list usually not greater than sys_minclock
	 * peers. Note that the head of the list is the system peer at
	 * the lowest stratum and that unsynchronized peers cannot
	 * survive this far.
	 *
	 * While at it, count the number of leap warning bits found.
	 * This will be used later to vote the system leap warning bit.
	 * If a leap warning bit is found on a reference clock, the vote
	 * is always won.
	 */
	leap_vote = 0;
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		peer->unreach = 0;
		peer->new_status = CTL_PST_SEL_SYNCCAND;
		sys_survivors++;
		if (peer->leap == LEAP_ADDSECOND) {
			if (peer->flags & FLAG_REFCLOCK)
				leap_vote = nlist;
			else 
				leap_vote++;
		}
		if (peer->flags & FLAG_PREFER)
			sys_prefer = peer;
	}

	/*
	 * Unless there are at least sys_misane survivors, leave the
	 * building dark. Otherwise, do a clockhop dance. Ordinarily,
	 * use the first survivor on the survivor list. However, if the
	 * last selection is not first on the list, use it as long as
	 * it doesn't get too old or too ugly.
	 */
	if (nlist > 0 && nlist >= sys_minsane) {
		double	x;

		typesystem = peer_list[0];
		if (osys_peer == NULL || osys_peer == typesystem) {
			sys_clockhop = 0; 
		} else if ((x = fabs(typesystem->offset -
		    osys_peer->offset)) < sys_mindisp) {
			if (sys_clockhop == 0)
				sys_clockhop = sys_mindisp;
			else
				sys_clockhop *= .5;
#ifdef DEBUG
			if (debug)
				printf("select: clockhop %d %.6f %.6f\n",
				    j, x, sys_clockhop);
#endif
			if (fabs(x) < sys_clockhop)
				typesystem = osys_peer;
			else
				sys_clockhop = 0;
		} else {
			sys_clockhop = 0;
		}
	}

	/*
	 * Mitigation rules of the game. We have the pick of the
	 * litter in typesystem if any survivors are left. If
	 * there is a prefer peer, use its offset and jitter.
	 * Otherwise, use the combined offset and jitter of all kitters.
	 */
	if (typesystem != NULL) {
		if (sys_prefer == NULL) {
			typesystem->new_status = CTL_PST_SEL_SYSPEER;
			clock_combine(peer_list, sys_survivors);
			sys_jitter = SQRT(SQUARE(sys_jitter) +
			    SQUARE(seljitter));
		} else {
			typesystem = sys_prefer;
			sys_clockhop = 0;
			typesystem->new_status = CTL_PST_SEL_SYSPEER;
			sys_offset = typesystem->offset;
			sys_jitter = typesystem->jitter;
		}
#ifdef DEBUG
		if (debug)
			printf("select: combine offset %.9f jitter %.9f\n",
			    sys_offset, sys_jitter);
#endif
	}
#ifdef REFCLOCK
	/*
	 * If a PPS driver is lit and the combined offset is less than
	 * 0.4 s, select the driver as the PPS peer and use its offset
	 * and jitter. However, if this is the atom driver, use it only
	 * if there is a prefer peer or there are no survivors and none
	 * are required.
	 */
	if (typepps != NULL && fabs(sys_offset) < 0.4 &&
	    (typepps->refclktype != REFCLK_ATOM_PPS ||
	    (typepps->refclktype == REFCLK_ATOM_PPS && (sys_prefer !=
	    NULL || (typesystem == NULL && sys_minsane == 0))))) {
		typesystem = typepps;
		sys_clockhop = 0;
		typesystem->new_status = CTL_PST_SEL_PPS;
 		sys_offset = typesystem->offset;
		sys_jitter = typesystem->jitter;
#ifdef DEBUG
		if (debug)
			printf("select: pps offset %.9f jitter %.9f\n",
			    sys_offset, sys_jitter);
#endif
	}
#endif /* REFCLOCK */

	/*
	 * If there are no survivors at this point, there is no
	 * system peer. If so and this is an old update, keep the
	 * current statistics, but do not update the clock.
	 */
	if (typesystem == NULL) {
		if (osys_peer != NULL)
			report_event(EVNT_NOPEER, NULL, NULL);
		sys_peer = NULL;			
		for (n = 0; n < NTP_HASH_SIZE; n++)
			for (peer = peer_hash[n]; peer != NULL; peer =
			    peer->next)
				peer->status = peer->new_status;
		return;
	}

	/*
	 * Do not use old data, as this may mess up the clock discipline
	 * stability.
	 */
	if (typesystem->epoch <= sys_epoch)
		return;

	/*
	 * We have found the alpha male. Wind the clock.
	 */
	if (osys_peer != typesystem)
		report_event(PEVNT_NEWPEER, typesystem, NULL);
	for (n = 0; n < NTP_HASH_SIZE; n++)
		for (peer = peer_hash[n]; peer != NULL; peer =
		    peer->next)
			peer->status = peer->new_status;
	clock_update(typesystem);
}


/*
 * clock_combine - compute system offset and jitter from selected peers
 */
static void
clock_combine(
	struct peer **peers,	/* survivor list */
	int	npeers		/* number of survivors */
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
	struct peer *peer	/* peer structure pointer */
	)
{
	double	dtemp;

	/*
	 * Careful squeak here. The value returned must be greater than
	 * the minimum root dispersion in order to avoid clockhop with
	 * highly precise reference clocks. Note that the root distance
	 * cannot exceed the sys_maxdist, as this is the cutoff by the
	 * selection algorithm.
	 */
	dtemp = (peer->delay + peer->rootdelay) / 2 + peer->disp +
	    peer->rootdisp + clock_phi * (current_time - peer->update) +
	    peer->jitter;
	if (dtemp < sys_mindisp)
		dtemp = sys_mindisp;
	return (dtemp);
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
	l_fp	xmt_tx, xmt_ty;

	if (!peer->dstadr)	/* drop peers without interface */
		return;

	xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap, peer->version,
	    peer->hmode);
	xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
	xpkt.ppoll = peer->hpoll;
	xpkt.precision = sys_precision;
	xpkt.refid = sys_refid;
	xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
	xpkt.rootdisp =  HTONS_FP(DTOUFP(sys_rootdisp));
	HTONL_FP(&sys_reftime, &xpkt.reftime);
	HTONL_FP(&peer->rec, &xpkt.org);
	HTONL_FP(&peer->dst, &xpkt.rec);

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
#ifdef OPENSSL
	if (!(peer->flags & FLAG_SKEY) && peer->keyid == 0) {
#else
	if (peer->keyid == 0) {
#endif /* OPENSSL */

		/*
		 * Transmit a-priori timestamps
		 */
		get_systime(&xmt_tx);
		if (peer->flip == 0) {	/* basic mode */
			peer->aorg = xmt_tx;
			HTONL_FP(&xmt_tx, &xpkt.xmt);
		} else {		/* interleaved modes */
			if (peer->hmode == MODE_BROADCAST) { /* bcst */
				HTONL_FP(&xmt_tx, &xpkt.xmt);
				if (peer->flip > 0)
					HTONL_FP(&peer->borg,
					    &xpkt.org);
				else
					HTONL_FP(&peer->aorg,
					    &xpkt.org);
			} else {	/* symmetric */
				if (peer->flip > 0)
					HTONL_FP(&peer->borg,
					    &xpkt.xmt);
				else
					HTONL_FP(&peer->aorg,
					    &xpkt.xmt);
			}
		}
		peer->t21_bytes = sendlen;
		sendpkt(&peer->srcadr, peer->dstadr, sys_ttl[peer->ttl],
		    &xpkt, sendlen);
		peer->sent++;
		peer->throttle += (1 << peer->minpoll) - 2;

		/*
		 * Capture a-posteriori timestamps
		 */
		get_systime(&xmt_ty);
		if (peer->flip != 0) {		/* interleaved modes */
			if (peer->flip > 0)
				peer->aorg = xmt_ty;
			else
				peer->borg = xmt_ty;
			peer->flip = -peer->flip;
		}
		L_SUB(&xmt_ty, &xmt_tx);
		LFPTOD(&xmt_ty, peer->xleave);
#ifdef DEBUG
		if (debug)
			printf("transmit: at %ld %s->%s mode %d len %d\n",
		    	    current_time, peer->dstadr ?
			    stoa(&peer->dstadr->sin) : "-",
		            stoa(&peer->srcadr), peer->hmode, sendlen);
#endif
		return;
	}

	/*
	 * Authentication is enabled, so the transmitted packet must be
	 * authenticated. If autokey is enabled, fuss with the various
	 * modes; otherwise, symmetric key cryptography is used.
	 */
#ifdef OPENSSL
	if (peer->flags & FLAG_SKEY) {
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
		 * one command and one or more responses.
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
			 * key cache, a birthday has happened or the key
			 * has expired, so the pseudo-random sequence is
			 * broken. In that case, purge the keylist and
			 * regenerate it.
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
				    CRYPTO_RESP, peer->associd, NULL);
			else
				exten = crypto_args(peer, CRYPTO_ASSOC |
				    CRYPTO_RESP, peer->associd, NULL);
			break;

		/*
		 * In symmetric modes the parameter, certificate, 
		 * identity, cookie and autokey exchanges are
		 * required. The leapsecond exchange is optional. But, a
		 * peer will not believe the other peer until the other
		 * peer has synchronized, so the certificate exchange
		 * might loop until then. If a peer finds a broken
		 * autokey sequence, it uses the autokey exchange to
		 * retrieve the autokey values. In any case, if a new
		 * keylist is generated, the autokey values are pushed.
		 */
		case MODE_ACTIVE:
		case MODE_PASSIVE:

			/*
			 * Parameter, certificate and identity.
			 */
			if (!peer->crypto)
				exten = crypto_args(peer, CRYPTO_ASSOC,
				    peer->associd, sys_hostname);
			else if (!(peer->crypto & CRYPTO_FLAG_CERT))
				exten = crypto_args(peer, CRYPTO_CERT,
				    peer->associd, peer->issuer);
			else if (!(peer->crypto & CRYPTO_FLAG_VRFY))
				exten = crypto_args(peer,
				    crypto_ident(peer), peer->associd,
				    NULL);

			/*
			 * Cookie and autokey. We request the cookie
			 * only when the this peer and the other peer
			 * are synchronized. But, this peer needs the
			 * autokey values when the cookie is zero. Any
			 * time we regenerate the key list, we offer the
			 * autokey values without being asked. If for
			 * some reason either peer finds a broken
			 * autokey sequence, the autokey exchange is
			 * used to retrieve the autokey values.
			 */
			else if (sys_leap != LEAP_NOTINSYNC &&
			    peer->leap != LEAP_NOTINSYNC &&
			    !(peer->crypto & CRYPTO_FLAG_COOK))
				exten = crypto_args(peer, CRYPTO_COOK,
				    peer->associd, NULL);
			else if (!(peer->crypto & CRYPTO_FLAG_AUTO))
				exten = crypto_args(peer, CRYPTO_AUTO,
				    peer->associd, NULL);
			else if (peer->flags & FLAG_ASSOC &&
			    peer->crypto & CRYPTO_FLAG_SIGN)
				exten = crypto_args(peer, CRYPTO_AUTO |
				    CRYPTO_RESP, peer->assoc, NULL);

			/*
			 * Wait for clock sync, then sign the
			 * certificate and retrieve the leapsecond
			 * values.
			 */
			else if (sys_leap == LEAP_NOTINSYNC)
				break;

			else if (!(peer->crypto & CRYPTO_FLAG_SIGN))
				exten = crypto_args(peer, CRYPTO_SIGN,
				    peer->associd, sys_hostname);
			else if (!(peer->crypto & CRYPTO_FLAG_LEAP))
				exten = crypto_args(peer, CRYPTO_LEAP,
				    peer->associd, NULL);
			break;

		/*
		 * In client mode the parameter, certificate, identity,
		 * cookie and sign exchanges are required. The
		 * leapsecond exchange is optional. If broadcast client
		 * mode the same exchanges are required, except that the
		 * autokey exchange is substitutes for the cookie
		 * exchange, since the cookie is always zero. If the
		 * broadcast client finds a broken autokey sequence, it
		 * uses the autokey exchange to retrieve the autokey
		 * values.
		 */
		case MODE_CLIENT:

			/*
			 * Parameter, certificate and identity.
			 */
			if (!peer->crypto)
				exten = crypto_args(peer, CRYPTO_ASSOC,
				    peer->associd, sys_hostname);
			else if (!(peer->crypto & CRYPTO_FLAG_CERT))
				exten = crypto_args(peer, CRYPTO_CERT,
				    peer->associd, peer->issuer);
			else if (!(peer->crypto & CRYPTO_FLAG_VRFY))
				exten = crypto_args(peer,
				    crypto_ident(peer), peer->associd,
				    NULL);

			/*
			 * Cookie and autokey. These are requests, but
			 * we use the peer association ID with autokey
			 * rather than our own.
			 */
			else if (!(peer->crypto & CRYPTO_FLAG_COOK))
				exten = crypto_args(peer, CRYPTO_COOK,
				    peer->associd, NULL);
			else if (!(peer->crypto & CRYPTO_FLAG_AUTO))
				exten = crypto_args(peer, CRYPTO_AUTO,
				    peer->assoc, NULL);

			/*
			 * Wait for clock sync, then sign the
			 * certificate and retrieve the leapsecond
			 * values.
			 */
			else if (sys_leap == LEAP_NOTINSYNC)
				break;

			else if (!(peer->crypto & CRYPTO_FLAG_SIGN))
				exten = crypto_args(peer, CRYPTO_SIGN,
				    peer->associd, sys_hostname);
			else if (!(peer->crypto & CRYPTO_FLAG_LEAP))
				exten = crypto_args(peer, CRYPTO_LEAP,
				    peer->associd, NULL);
			break;
		}

		/*
		 * Add a queued extension field if present. This is
		 * always a request message, so the reply ID is already
		 * in the message. If an error occurs, the error bit is
		 * lit in the response.
		 */
		if (peer->cmmd != NULL) {
			u_int32 temp32;

			temp32 = CRYPTO_RESP;
			peer->cmmd->opcode |= htonl(temp32);
			sendlen += crypto_xmit(peer, &xpkt, NULL,
			    sendlen, peer->cmmd, 0);
			free(peer->cmmd);
			peer->cmmd = NULL;
		}

		/*
		 * Add an extension field created above. All but the
		 * autokey response message are request messages.
		 */
		if (exten != NULL) {
			if (exten->opcode != 0)
				sendlen += crypto_xmit(peer, &xpkt,
				    NULL, sendlen, exten, 0);
			free(exten);
		}

		/*
		 * Calculate the next session key. Since extension
		 * fields are present, the cookie value is zero.
		 */
		if (sendlen > LEN_PKT_NOMAC) {
			session_key(&peer->dstadr->sin, &peer->srcadr,
			    xkeyid, 0, 2);
		}
	} 
#endif /* OPENSSL */

	/*
	 * Transmit a-priori timestamps
	 */
	get_systime(&xmt_tx);
	if (peer->flip == 0) {		/* basic mode */
		peer->aorg = xmt_tx;
		HTONL_FP(&xmt_tx, &xpkt.xmt);
	} else {			/* interleaved modes */
		if (peer->hmode == MODE_BROADCAST) { /* bcst */
			HTONL_FP(&xmt_tx, &xpkt.xmt);
			if (peer->flip > 0)
				HTONL_FP(&peer->borg, &xpkt.org);
			else
				HTONL_FP(&peer->aorg, &xpkt.org);
		} else {		/* symmetric */
			if (peer->flip > 0)
				HTONL_FP(&peer->borg, &xpkt.xmt);
			else
				HTONL_FP(&peer->aorg, &xpkt.xmt);
		}
	}
	xkeyid = peer->keyid;
	authlen = authencrypt(xkeyid, (u_int32 *)&xpkt, sendlen);
	if (authlen == 0) {
		report_event(PEVNT_AUTH, peer, "no key");
		peer->flash |= TEST5;		/* auth error */
		peer->badauth++;
		return;
	}
	sendlen += authlen;
#ifdef OPENSSL
	if (xkeyid > NTP_MAXKEY)
		authtrust(xkeyid, 0);
#endif /* OPENSSL */
	if (sendlen > sizeof(xpkt)) {
		msyslog(LOG_ERR, "proto: buffer overflow %u", sendlen);
		exit (-1);
	}
	peer->t21_bytes = sendlen;
	sendpkt(&peer->srcadr, peer->dstadr, sys_ttl[peer->ttl], &xpkt,
	    sendlen);
	peer->sent++;
	peer->throttle += (1 << peer->minpoll) - 2;

	/*
	 * Capture a-posteriori timestamps
	 */
	get_systime(&xmt_ty);
	if (peer->flip != 0) {			/* interleaved modes */
		if (peer->flip > 0)
			peer->aorg = xmt_ty;
		else
			peer->borg = xmt_ty;
		peer->flip = -peer->flip;
	}
	L_SUB(&xmt_ty, &xmt_tx);
	LFPTOD(&xmt_ty, peer->xleave);
#ifdef OPENSSL
#ifdef DEBUG
	if (debug)
		printf("transmit: at %ld %s->%s mode %d keyid %08x len %d index %d\n",
		    current_time, peer->dstadr ?
		    ntoa(&peer->dstadr->sin) : "-",
	 	    ntoa(&peer->srcadr), peer->hmode, xkeyid, sendlen,
		    peer->keynumber);
#endif
#else /* OPENSSL */
#ifdef DEBUG
	if (debug)
		printf("transmit: at %ld %s->%s mode %d keyid %08x len %d\n",
		    current_time, peer->dstadr ?
		    ntoa(&peer->dstadr->sin) : "-",
		    ntoa(&peer->srcadr), peer->hmode, xkeyid, sendlen);
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
	int	xmode,		/* receive mode */
	keyid_t	xkeyid,		/* transmit key ID */
	int	flags		/* restrict mask */
	)
{
	struct pkt xpkt;	/* transmit packet structure */
	struct pkt *rpkt;	/* receive packet structure */
	l_fp	xmt_tx, xmt_ty;
	int	sendlen;
#ifdef OPENSSL
	u_int32	temp32;
#endif

	/*
	 * Initialize transmit packet header fields from the receive
	 * buffer provided. We leave the fields intact as received, but
	 * set the peer poll at the maximum of the receive peer poll and
	 * the system minimum poll (ntp_minpoll). This is for KoD rate
	 * control and not strictly specification compliant, but doesn't
	 * break anything.
	 *
	 * If the gazinta was from a multicast address, the gazoutta
	 * must go out another way.
	 */
	rpkt = &rbufp->recv_pkt;
	if (rbufp->dstadr->flags & INT_MCASTOPEN)
		rbufp->dstadr = findinterface(&rbufp->recv_srcadr);

	/*
	 * If this is a kiss-o'-death (KoD) packet, show leap
	 * unsynchronized, stratum zero, reference ID the four-character
	 * kiss code and system root delay. Note we don't reveal the
	 * local time, so these packets can't be used for
	 * synchronization.
	 */
	if (flags & RES_KOD) {
		sys_kodsent++;
		xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOTINSYNC,
		    PKT_VERSION(rpkt->li_vn_mode), xmode);
		xpkt.stratum = STRATUM_PKT_UNSPEC;
		xpkt.ppoll = max(rpkt->ppoll, ntp_minpoll);
		memcpy(&xpkt.refid, "RATE", 4);
		xpkt.org = rpkt->xmt;
		xpkt.rec = rpkt->xmt;
		xpkt.xmt = rpkt->xmt;

	/*
	 * This is a normal packet. Use the system variables.
	 */
	} else {
		xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
		    PKT_VERSION(rpkt->li_vn_mode), xmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
		xpkt.ppoll = max(rpkt->ppoll, ntp_minpoll);
		xpkt.precision = sys_precision;
		xpkt.refid = sys_refid;
		xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
		xpkt.rootdisp = HTONS_FP(DTOUFP(sys_rootdisp));
		HTONL_FP(&sys_reftime, &xpkt.reftime);
		xpkt.org = rpkt->xmt;
		HTONL_FP(&rbufp->recv_time, &xpkt.rec);
		get_systime(&xmt_tx);
		HTONL_FP(&xmt_tx, &xpkt.xmt);
	}

#ifdef HAVE_NTP_SIGND
	if (flags & RES_MSSNTP) {
		send_via_ntp_signd(rbufp, xmode, xkeyid, flags, &xpkt);
		return;
	}
#endif /* HAVE_NTP_SIGND */

	/*
	 * If the received packet contains a MAC, the transmitted packet
	 * is authenticated and contains a MAC. If not, the transmitted
	 * packet is not authenticated.
	 */
	sendlen = LEN_PKT_NOMAC;
	if (rbufp->recv_length == sendlen) {
		sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, 0, &xpkt,
		    sendlen);
#ifdef DEBUG
		if (debug)
			printf(
			    "transmit: at %ld %s->%s mode %d len %d\n",
			    current_time, stoa(&rbufp->dstadr->sin),
			    stoa(&rbufp->recv_srcadr), xmode, sendlen);
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
		if (rbufp->recv_length > sendlen + MAX_MAC_LEN) {
			session_key(&rbufp->dstadr->sin,
			    &rbufp->recv_srcadr, xkeyid, 0, 2);
			temp32 = CRYPTO_RESP;
			rpkt->exten[0] |= htonl(temp32);
			sendlen += crypto_xmit(NULL, &xpkt, rbufp,
			    sendlen, (struct exten *)rpkt->exten,
			    cookie);
		} else {
			session_key(&rbufp->dstadr->sin,
			    &rbufp->recv_srcadr, xkeyid, cookie, 2);
		}
	}
#endif /* OPENSSL */
	get_systime(&xmt_tx);
	sendlen += authencrypt(xkeyid, (u_int32 *)&xpkt, sendlen);
#ifdef OPENSSL
	if (xkeyid > NTP_MAXKEY)
		authtrust(xkeyid, 0);
#endif /* OPENSSL */
	sendpkt(&rbufp->recv_srcadr, rbufp->dstadr, 0, &xpkt, sendlen);
	get_systime(&xmt_ty);
	L_SUB(&xmt_ty, &xmt_tx);
	sys_authdelay = xmt_ty;
#ifdef DEBUG
	if (debug)
		printf(
		    "transmit: at %ld %s->%s mode %d keyid %08x len %d\n",
		    current_time, ntoa(&rbufp->dstadr->sin),
		    ntoa(&rbufp->recv_srcadr), xmode, xkeyid, sendlen);
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
	peer->flags &= ~FLAG_ASSOC;
#ifdef DEBUG
	if (debug)
		printf("key_expire: at %lu associd %d\n", current_time,
		    peer->associd);
#endif
}
#endif /* OPENSSL */


/*
 * local_refid(peer) - check peer refid to avoid selecting peers
 *		       currently synced to this ntpd.
 */
static int
local_refid(
	struct peer *	p
	)
{
	endpt *	unicast_ep;

	if (p->dstadr != NULL && !(INT_MCASTIF & p->dstadr->flags))
		unicast_ep = p->dstadr;
	else
		unicast_ep = findinterface(&p->srcadr);

	if (unicast_ep != NULL && p->refid == unicast_ep->addr_refid)
		return TRUE;
	else
		return FALSE;
}


/*
 * Determine if the peer is unfit for synchronization
 *
 * A peer is unfit for synchronization if
 * > TEST10 bad leap or stratum below floor or at or above ceiling
 * > TEST11 root distance exceeded for remote peer
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
	 * greater than or equal to the ceiling.
	 */
	if (peer->leap == LEAP_NOTINSYNC || peer->stratum < sys_floor ||
	    peer->stratum >= sys_ceiling)
		rval |= TEST10;		/* bad synch or stratum */

	/*
	 * A distance error for a remote peer occurs if the root
	 * distance is greater than or equal to the distance threshold
	 * plus the increment due to one host poll interval.
	 */
	if (!(peer->flags & FLAG_REFCLOCK) && root_distance(peer) >=
	    sys_maxdist + clock_phi * ULOGTOD(peer->hpoll))
		rval |= TEST11;		/* distance exceeded */

	/*
	 * A loop error occurs if the remote peer is synchronized to the
	 * local peer or if the remote peer is synchronized to the same
	 * server as the local peer but only if the remote peer is
	 * neither a reference clock nor an orphan.
	 */
	if (peer->stratum > 1 && local_refid(peer))
		rval |= TEST12;		/* synchronization loop */

	/*
	 * An unreachable error occurs if the server is unreachable or
	 * the noselect bit is set.
	 */
	if (!peer->reach || (peer->flags & FLAG_NOSELECT))
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
 * This routine measures the system precision defined as the minimum of
 * a sequence of differences between successive readings of the system
 * clock. However, if a difference is less than MINSTEP, the clock has
 * been read more than once during a clock tick and the difference is
 * ignored. We set MINSTEP greater than zero in case something happens
 * like a cache miss.
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
	 * Loop to find precision value in seconds.
	 */
	tick = MAXSTEP;
	i = 0;
	get_systime(&last);
	while (1) {
		get_systime(&val);
		diff = val;
		L_SUB(&diff, &last);
		last = val;
		LFPTOD(&diff, dtemp);
		if (dtemp < MINSTEP)
			continue;

		if (dtemp < tick)
			tick = dtemp;
		if (++i >= MINLOOPS)
			break;
	}
	sys_tick = tick;

	/*
	 * Find the nearest power of two.
	 */
	msyslog(LOG_NOTICE, "proto: precision = %.3f usec", tick * 1e6);
	for (i = 0; tick <= 1; i++)
		tick *= 2;
	if (tick - 1 > 1 - tick / 2)
		i--;
	return (-i);
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
	 * broadcasting, require authentication.
	 */
	sys_leap = LEAP_NOTINSYNC;
	sys_stratum = STRATUM_UNSPEC;
	memcpy(&sys_refid, "INIT", 4);
	sys_peer = NULL;
	sys_rootdelay = 0;
	sys_rootdisp = 0;
	L_CLR(&sys_reftime);
	sys_jitter = 0;
	sys_precision = (s_char)default_get_precision();
	get_systime(&dummy);
	sys_survivors = 0;
	sys_manycastserver = 0;
	sys_bclient = 0;
	sys_bdelay = 0;
	sys_authenticate = 1;
	sys_stattime = current_time;
	proto_clr_stats();
	for (i = 0; i < MAX_TTL; i++) {
		sys_ttl[i] = (u_char)((i * 256) / MAX_TTL);
		sys_ttlmax = i;
	}
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
	sockaddr_u *svalue
	)
{
	/*
	 * Figure out what he wants to change, then do it
	 */
	DPRINTF(2, ("proto_config: code %d value %lu dvalue %lf\n",
		    item, value, dvalue));

	switch (item) {

	/*
	 * enable and disable commands - arguments are Boolean.
	 */
	case PROTO_AUTHENTICATE: /* authentication (auth) */
		sys_authenticate = value;
		break;

	case PROTO_BROADCLIENT: /* broadcast client (bclient) */
		sys_bclient = (int)value;
		if (sys_bclient == 0)
			io_unsetbclient();
		else
			io_setbclient();
		break;

#ifdef REFCLOCK
	case PROTO_CAL:		/* refclock calibrate (calibrate) */
		cal_enable = value;
		break;
#endif /* REFCLOCK */

	case PROTO_KERNEL:	/* kernel discipline (kernel) */
		kern_enable = value;
		break;

	case PROTO_MONITOR:	/* monitoring (monitor) */
		if (value)
			mon_start(MON_ON);
		else
			mon_stop(MON_ON);
		break;

	case PROTO_NTP:		/* NTP discipline (ntp) */
		ntp_enable = value;
		break;

	case PROTO_PPS:		/* PPS discipline (pps) */
		pps_enable = value;
		break;

	case PROTO_FILEGEN:	/* statistics (stats) */
		stats_control = value;
		break;

	/*
	 * tos command - arguments are double, sometimes cast to int
	 */
	case PROTO_BEACON:	/* manycast beacon (beacon) */
		sys_beacon = (int)dvalue;
		break;

	case PROTO_BROADDELAY:	/* default broadcast delay (bdelay) */
		sys_bdelay = dvalue;
		break;

	case PROTO_CEILING:	/* stratum ceiling (ceiling) */
		sys_ceiling = (int)dvalue;
		break;

	case PROTO_COHORT:	/* cohort switch (cohort) */
		sys_cohort = (int)dvalue;
		break;

	case PROTO_FLOOR:	/* stratum floor (floor) */
		sys_floor = (int)dvalue;
		break;

	case PROTO_MAXCLOCK:	/* maximum candidates (maxclock) */
		sys_maxclock = (int)dvalue;
		break;

	case PROTO_MAXDIST:	/* select threshold (maxdist) */
		sys_maxdist = dvalue;
		break;

	case PROTO_CALLDELAY:	/* modem call delay (mdelay) */
		break;		/* NOT USED */

	case PROTO_MINCLOCK:	/* minimum candidates (minclock) */
		sys_minclock = (int)dvalue;
		break;

	case PROTO_MINDISP:	/* minimum distance (mindist) */
		sys_mindisp = dvalue;
		break;

	case PROTO_MINSANE:	/* minimum survivors (minsane) */
		sys_minsane = (int)dvalue;
		break;

	case PROTO_ORPHAN:	/* orphan stratum (orphan) */
		sys_orphan = (int)dvalue;
		break;

	case PROTO_ADJ:		/* tick increment (tick) */
		sys_tick = dvalue;
		break;

	/*
	 * Miscellaneous commands
	 */
	case PROTO_MULTICAST_ADD: /* add group address */
		if (svalue != NULL)
			io_multicast_add(svalue);
		sys_bclient = 1;
		break;

	case PROTO_MULTICAST_DEL: /* delete group address */
		if (svalue != NULL)
			io_multicast_del(svalue);
		break;

	default:
		msyslog(LOG_NOTICE,
		    "proto: unsupported option %d", item);
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
	sys_newversion = 0;
	sys_oldversion = 0;
	sys_declined = 0;
	sys_restricted = 0;
	sys_badlength = 0;
	sys_badauth = 0;
	sys_limitrejected = 0;
}
