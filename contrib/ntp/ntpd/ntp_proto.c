/*
 * ntp_proto.c - NTP version 4 protocol machinery
 *
 * $FreeBSD$
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ntpd.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"
#include "ntp_control.h"
#include "ntp_string.h"
#include "ntp_crypto.h"

#include <stdio.h>

#if defined(VMS) && defined(VMS_LOCALUNIT)	/*wjm*/
#include "ntp_refclock.h"
#endif

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <sys/sysctl.h>
#endif

/*
 * System variables are declared here. See Section 3.2 of the
 * specification.
 */
u_char	sys_leap;		/* system leap indicator */
u_char	sys_stratum;		/* stratum of system */
s_char	sys_precision;		/* local clock precision */
double	sys_rootdelay;		/* roundtrip delay to primary source */
double	sys_rootdispersion;	/* dispersion to primary source */
u_int32 sys_refid;		/* reference source for local clock */
static	double sys_offset;	/* current local clock offset */
l_fp	sys_reftime;		/* time we were last updated */
struct	peer *sys_peer; 	/* our current peer */
struct	peer *sys_prefer;	/* our cherished peer */
#ifdef AUTOKEY
u_long	sys_automax;		/* maximum session key lifetime */
#endif /* AUTOKEY */

/*
 * Nonspecified system state variables.
 */
int	sys_bclient;		/* we set our time to broadcasts */
double	sys_bdelay; 		/* broadcast client default delay */
int	sys_authenticate;	/* requre authentication for config */
l_fp	sys_authdelay;		/* authentication delay */
static	u_long sys_authdly[2]; 	/* authentication delay shift reg */
static	u_char leap_consensus;	/* consensus of survivor leap bits */
static	double sys_selerr; 	/* select error (squares) */
static	double sys_syserr;	/* system error (squares) */
keyid_t	sys_private;		/* private value for session seed */
int	sys_manycastserver;	/* respond to manycast client pkts */
u_int sys_survivors;		/* truest of the truechimers */
int	peer_ntpdate;		/* active peers in ntpdate mode */
#ifdef AUTOKEY
char	*sys_hostname;		/* gethostname() name */
#endif /* AUTOKEY */

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
static	void	fast_xmit	P((struct recvbuf *, int, keyid_t, int));
static	void	clock_update	P((void));
int	default_get_precision	P((void));


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
		 * The polling state machine. There are two kinds of
		 * machines, those that never expect a reply (broadcast
		 * and manycast server modes) and those that do (all
		 * other modes). The dance is intricate...
		 */
		if (peer->cast_flags & (MDF_BCAST | MDF_MCAST)) {

			/*
			 * In broadcast mode the poll interval is fixed
			 * at minpoll and the ttl at ttlmax.
			 */
			hpoll = peer->minpoll;
			peer->ttl = peer->ttlmax;
#ifdef AUTOKEY
		} else if (peer->cast_flags & MDF_ACAST) {

			/*
			 * In manycast mode we start with the minpoll
			 * interval and ttl. However, the actual poll
			 * interval is eight times the nominal poll
			 * interval shown here. If fewer than three
			 * servers are found, the ttl is increased by
			 * one and we try again. If this continues to
			 * the max ttl, the poll interval is bumped by
			 * one and we try again. If at least three
			 * servers are found, the poll interval
			 * increases with the system poll interval to
			 * the max and we continue indefinately.
			 * However, about once per day when the
			 * agreement parameters are refreshed, the
			 * manycast clients are reset and we start from
			 * the beginning. This is to catch and clamp the
			 * ttl to the lowest practical value and avoid
			 * knocking on spurious doors.
			 */
			if (sys_survivors < NTP_MINCLOCK && peer->ttl <
			    peer->ttlmax)
				peer->ttl++;
			hpoll = sys_poll;
#endif /* AUTOKEY */
		} else {

			/*
			 * For associations expecting a reply, the
			 * watchdog counter is bumped by one if the peer
			 * has not been heard since the previous poll.
			 * If the counter reaches the max, the peer is
			 * demobilized if not configured and just
			 * cleared if it is, but in this case the poll
			 * interval is bumped by one.
			 */
			if (peer->unreach < NTP_UNREACH) {
				peer->unreach++;
			} else if (!(peer->flags & FLAG_CONFIG)) {
				unpeer(peer);
				clock_select();
				return;

			} else {
				peer_clear(peer);
				hpoll++;
			}
		}
		oreach = peer->reach;
		peer->reach <<= 1;
		if (peer->reach == 0) {

			/*
			 * If this association has become unreachable,
			 * clear it and raise a trap.
			 */
			if (oreach != 0) {
				report_event(EVNT_UNREACH, peer);
				peer->timereachable = current_time;
				if (!(peer->flags & FLAG_CONFIG)) {
					unpeer(peer);
					clock_select();
					return;
				} else {
					peer_clear(peer);
					hpoll = peer->minpoll;
				}
			}
			if (peer->flags & FLAG_IBURST)
				peer->burst = NTP_SHIFT;
		} else {

			/*
			 * Here the peer is reachable. If it has not
			 * been heard for three consecutive polls, stuff
			 * the clock filter. Next, determine the poll
			 * interval. If the peer is a synchronization
			 * candidate, use the system poll interval. If
			 * the peer is not sane, increase it by one. If
			 * the number of valid updates is not greater
			 * than half the register size, clamp it to the
			 * minimum. This is to quickly recover the time
			 * variables when a noisy peer shows life.
			 */
			if (!(peer->reach & 0x07)) {
				clock_filter(peer, 0., 0., MAXDISPERSE);
				clock_select();
			}
			if ((peer->stratum > 1 && peer->refid ==
			    peer->dstadr->sin.sin_addr.s_addr) ||
			    peer->stratum >= STRATUM_UNSPEC)
				hpoll++;
			else
				hpoll = sys_poll;
			if (peer->flags & FLAG_BURST)
				peer->burst = NTP_SHIFT;
		}
	} else {
		peer->burst--;
		if (peer->burst == 0) {

			/*
			 * If a broadcast client at this point, the
			 * burst has concluded, so we switch to client
			 * mode and purge the keylist, since no further
			 * transmissions will be made.
			 */
			if (peer->cast_flags & MDF_BCLNT) {
				peer->hmode = MODE_BCLIENT;
#ifdef AUTOKEY
				key_expire(peer);
#endif /* AUTOKEY */
			}
			poll_update(peer, hpoll);
			clock_select();

			/*
			 * If ntpdate mode and the clock has not been
			 * set and all peers have completed the burst,
			 * we declare a successful failure.
			 */
			if (mode_ntpdate) {
				peer_ntpdate--;
				if (peer_ntpdate > 0)
					return;
				NLOG(NLOG_SYNCEVENT | NLOG_SYSEVENT)
				    msyslog(LOG_NOTICE,
				    "no reply; clock not set");
				printf(
				    "ntpd: no reply; clock not set\n");
				exit(0);
			}
			return;

		}
	}
	peer->outdate = current_time;
	poll_update(peer, hpoll);

	/*
	 * We need to be very careful about honking uncivilized time.
	 * Never transmit if in broadcast client mode or access denied.
	 * If in broadcast mode, transmit only if synchronized to a
	 * valid source. 
	 */
	if (peer->hmode == MODE_BCLIENT || peer->flash & TEST4) {
		return;
	} else if (peer->hmode == MODE_BROADCAST) {
		if (sys_peer == NULL)
			return;
	}
	peer_xmit(peer);
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
	int has_mac;			/* length of MAC field */
	int authlen;			/* offset of MAC field */
	int is_authentic;		/* cryptosum ok */
	keyid_t skeyid;			/* cryptographic keys */
	struct sockaddr_in *dstadr_sin;	/* active runway */
#ifdef AUTOKEY
	keyid_t pkeyid, tkeyid;		/* cryptographic keys */
#endif /* AUTOKEY */
	struct peer *peer2;
	int retcode = AM_NOMATCH;

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
	ntp_monitor(rbufp);
	restrict_mask = restrictions(&rbufp->recv_srcadr);
#ifdef DEBUG
	if (debug > 2)
		printf("receive: at %ld %s<-%s restrict %02x\n",
		    current_time, ntoa(&rbufp->dstadr->sin),
		    ntoa(&rbufp->recv_srcadr), restrict_mask);
#endif
	if (restrict_mask & RES_IGNORE)
		return;				/* no anything */
	if (!(SRCPORT(&rbufp->recv_srcadr) == NTP_PORT ||
	    SRCPORT(&rbufp->recv_srcadr) >= IPPORT_RESERVED)) {
		sys_badlength++;
		return;				/* invalid port */
	}
	pkt = &rbufp->recv_pkt;
	if (PKT_VERSION(pkt->li_vn_mode) == NTP_VERSION) {
		sys_newversionpkt++;		/* new version */
	} else if (!(restrict_mask & RES_VERSION) &&
	    PKT_VERSION(pkt->li_vn_mode) >= NTP_OLDVERSION) {
		sys_oldversionpkt++;		/* old version */
	} else {
		sys_unknownversion++;
		return;				/* invalid version */
	}
	if (PKT_MODE(pkt->li_vn_mode) == MODE_PRIVATE) {
		if (restrict_mask & RES_NOQUERY)
			return;			/* no query private */
		process_private(rbufp, ((restrict_mask &
		    RES_NOMODIFY) == 0));
		return;
	}
	if (PKT_MODE(pkt->li_vn_mode) == MODE_CONTROL) {
		if (restrict_mask & RES_NOQUERY)
			return;			/* no query control */
		process_control(rbufp, restrict_mask);
		return;
	}
	if (rbufp->recv_length < LEN_PKT_NOMAC) {
		sys_badlength++;
		return;				/* runt packet */
	}

	/*
	 * Validate mode. Note that NTPv1 is no longer supported.
	 */
	hismode = (int)PKT_MODE(pkt->li_vn_mode);
	if (hismode == MODE_UNSPEC) {
		sys_badlength++;
		return;				/* invalid mode */
	}

	/*
	 * Discard broadcast packets received on the wildcard interface
	 * or if not enabled as broadcast client.
	 */
	if (PKT_MODE(pkt->li_vn_mode) == MODE_BROADCAST &&
	    (rbufp->dstadr == any_interface || !sys_bclient))
		return;

	/*
	 * Parse the extension field if present. We figure out whether
	 * an extension field is present by measuring the MAC size. If
	 * the number of words following the packet header is 0 or 1, no
	 * MAC is present and the packet is not authenticated. If 1, the
	 * packet is a reply to a previous request that failed to
	 * authenticate. If 3, the packet is authenticated with DES; if
	 * 5, the packet is authenticated with MD5. If greater than 5,
	 * an extension field is present. If 2 or 4, the packet is a
	 * runt and goes poof! with a brilliant flash.
	 */
	skeyid = 0;
#ifdef AUTOKEY
	pkeyid = tkeyid = 0;
#endif /* AUTOKEY */
	authlen = LEN_PKT_NOMAC;
	while ((has_mac = rbufp->recv_length - authlen) > 0) {
		int temp;

		if (has_mac % 4 != 0 || has_mac < 0) {
			sys_badlength++;
			return;
		}
		if (has_mac == 1 * 4 || has_mac == 3 * 4 || has_mac ==
		    MAX_MAC_LEN) {
			skeyid = ntohl(((u_int32 *)pkt)[authlen / 4]);
			break;

		} else if (has_mac > MAX_MAC_LEN) {
			temp = ntohl(((u_int32 *)pkt)[authlen / 4]) &
			    0xffff;
			if (temp < 4 || temp % 4 != 0) {
				sys_badlength++;
				return;
			}
			authlen += temp;
		} else {
			sys_badlength++;
			return;
		}
	}

	/*
	 * We have tossed out as many buggy packets as possible early in
	 * the game to reduce the exposure to a clogging attack. Now we
	 * have to burn some cycles to find the association and
	 * authenticate the packet if required. Note that we burn only
	 * MD5 or DES cycles, again to reduce exposure. There may be no
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
	peer = findpeer(&rbufp->recv_srcadr, rbufp->dstadr, rbufp->fd,
	    hismode, &retcode);
	is_authentic = 0;
	dstadr_sin = &rbufp->dstadr->sin;
	if (has_mac == 0) {
#ifdef DEBUG
		if (debug)
			printf("receive: at %ld %s<-%s mode %d code %d\n",
			    current_time, ntoa(&rbufp->dstadr->sin),
			    ntoa(&rbufp->recv_srcadr), hismode, retcode);
#endif
	} else {
#ifdef AUTOKEY
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
				 * mobilized.
				 */
				pkeyid = 0;
				if (rbufp->dstadr->bcast.sin_addr.s_addr
				    != 0)
					dstadr_sin =
					    &rbufp->dstadr->bcast;
			} else if (peer == NULL) {
				pkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin, 0,
				    sys_private, 0);
			} else {
				pkeyid = peer->pcookie.key;
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
#endif /* AUTOKEY */

		/*
		 * Compute the cryptosum. Note a clogging attack may
		 * succeed in bloating the key cache. If an autokey,
		 * purge it immediately, since we won't be needing it
		 * again.
		 */
		if (authdecrypt(skeyid, (u_int32 *)pkt, authlen,
		    has_mac))
			is_authentic = 1;
		else
			sys_badauth++;
#ifdef AUTOKEY
		if (skeyid > NTP_MAXKEY)
			authtrust(skeyid, 0);
#endif /* AUTOKEY */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d code %d keyid %08x len %d mac %d auth %d\n",
			    current_time, ntoa(dstadr_sin),
			    ntoa(&rbufp->recv_srcadr), hismode, retcode,
			    skeyid, authlen, has_mac,
			    is_authentic);
#endif
	}

	/*
	 * The association matching rules are implemented by a set of
	 * routines and a table in ntp_peer.c. A packet matching an
	 * association is processed by that association. If not and
	 * certain conditions prevail, then an ephemeral association is
	 * mobilized: a broadcast packet mobilizes a broadcast client
	 * aassociation; a server packet mobilizes a client association;
	 * a symmetric active packet mobilizes a symmetric passive
	 * association. And, the adventure continues...
	 */
	switch (retcode) {
	case AM_FXMIT:

		/*
		 * This is a client mode packet not matching a known
		 * association. If from a manycast client we run a few
		 * sanity checks before deciding to send a unicast
		 * server response. Otherwise, it must be a client
		 * request, so send a server response and go home.
		 */
		if (sys_manycastserver && (rbufp->dstadr->flags &
		    INT_MULTICAST)) {
	
			/*
			 * We are picky about responding to a
			 * manycaster. There is no reason to respond to
			 * a request if our time is worse than the
			 * manycaster. We certainly don't reply if not
			 * synchronized to proventic time.
			 */
			if (sys_peer == NULL)
				return;

			/*
			 * We don't reply if the our stratum is greater
			 * than the manycaster.
			 */ 
			if (PKT_TO_STRATUM(pkt->stratum) < sys_stratum)
				return;
		}

		/*
		 * Note that we don't require an authentication check
		 * here, since we can't set the system clock; but, we do
		 * set the key ID to zero to tell the caller about this.
		 */
		if (is_authentic)
			fast_xmit(rbufp, MODE_SERVER, skeyid,
			    restrict_mask);
		else
			fast_xmit(rbufp, MODE_SERVER, 0, restrict_mask);
		return;

	case AM_MANYCAST:

		/*
		 * This is a server mode packet returned in response to
		 * a client mode packet sent to a multicast group
		 * address. The originate timestamp is a good nonce to
		 * reliably associate the reply with what was sent. If
		 * there is no match, that's curious and could be an
		 * intruder attempting to clog, so we just ignore it.
		 *
		 * First, make sure the packet is authentic. If so and
		 * the manycast association is found, we mobilize a
		 * client mode association, copy pertinent variables
		 * from the manycast to the client mode association and
		 * wind up the spring.
		 *
		 * There is an implosion hazard at the manycast client,
		 * since the manycast servers send the server packet
		 * immediately.
		 */
		if ((restrict_mask & (RES_DONTSERVE | RES_LIMITED |
		    RES_NOPEER)) || (sys_authenticate &&
		    !is_authentic))
			return;

		peer2 = findmanycastpeer(rbufp);
		if (peer2 == 0)
			return;

		peer = newpeer(&rbufp->recv_srcadr, rbufp->dstadr,
		    MODE_CLIENT, PKT_VERSION(pkt->li_vn_mode),
		    sys_minpoll, NTP_MAXDPOLL, FLAG_IBURST |
		    (peer2->flags & (FLAG_AUTHENABLE | FLAG_SKEY)),
		    MDF_UCAST, 0, skeyid);
		if (peer == NULL)
			return;
		break;

	case AM_NEWPASS:

		/*
		 * This is the first packet received from a symmetric
		 * active peer. First, make sure the packet is
		 * authentic. If so, mobilize a symmetric passive
		 * association.
		 */
		if ((restrict_mask & (RES_DONTSERVE | RES_LIMITED |
		    RES_NOPEER)) || (sys_authenticate &&
		    !is_authentic)) {
			fast_xmit(rbufp, MODE_PASSIVE, 0,
			    restrict_mask);
			return;
		}
		peer = newpeer(&rbufp->recv_srcadr, rbufp->dstadr,
		    MODE_PASSIVE, PKT_VERSION(pkt->li_vn_mode),
	 	    sys_minpoll, NTP_MAXDPOLL, sys_authenticate ?
		    FLAG_AUTHENABLE : 0, MDF_UCAST, 0, skeyid);
		if (peer == NULL)
			return;
		break;

	case AM_NEWBCL:

		/*
		 * This is the first packet received from a broadcast
		 * server. First, make sure the packet is authentic, not
		 * restricted and that we are a broadcast or multicast
		 * client. If so, mobilize a broadcast client
		 * association.
		 */
		if ((restrict_mask & (RES_DONTSERVE | RES_LIMITED |
		    RES_NOPEER)) || (sys_authenticate &&
		    !is_authentic) || !sys_bclient)
			return;

		peer = newpeer(&rbufp->recv_srcadr, rbufp->dstadr,
		    MODE_CLIENT, PKT_VERSION(pkt->li_vn_mode),
		    sys_minpoll, NTP_MAXDPOLL, FLAG_MCAST |
		    FLAG_IBURST | (sys_authenticate ?
		    FLAG_AUTHENABLE : 0), MDF_BCLNT, 0, skeyid);
#ifdef AUTOKEY
#ifdef PUBKEY
		if (peer == NULL)
			return;
		if (peer->flags & FLAG_SKEY)
			crypto_recv(peer, rbufp);
#endif /* PUBKEY */
#endif /* AUTOKEY */
		return;

	case AM_POSSBCL:
	case AM_PROCPKT:

		/*
		 * Happiness and nothing broke. Earn some revenue.
		 */
		break;

	default:

		/*
		 * Invalid mode combination. Leave the island
		 * immediately.
		 */
#ifdef DEBUG
		if (debug)
			printf("receive: bad protocol %d\n", retcode);
#endif
		return;
	}

	/*
	 * If the peer isn't configured, set his authenable and autokey
	 * status based on the packet. Once the status is set, it can't
	 * be unset. It seems like a silly idea to do this here, rather
	 * in the configuration routine, but in some goofy cases the
	 * first packet sent cannot be authenticated and we need a way
	 * for the dude to change his mind.
	 */
	oflags = peer->flags;
	peer->timereceived = current_time;
	peer->received++;
	if (!(peer->flags & FLAG_CONFIG) && has_mac) {
		peer->flags |= FLAG_AUTHENABLE;
#ifdef AUTOKEY
		if (skeyid > NTP_MAXKEY)
			peer->flags |= FLAG_SKEY;
#endif /* AUTOKEY */
	}

	/*
	 * A valid packet must be from an authentic and allowed source.
	 * All packets must pass the authentication allowed tests.
	 * Autokey authenticated packets must pass additional tests and
	 * public-key authenticated packets must have the credentials
	 * verified. If all tests are passed, the packet is forwarded
	 * for processing. If not, the packet is discarded and the
	 * association demobilized if appropriate.
	 */
	peer->flash = 0;
	if (is_authentic) {
		peer->flags |= FLAG_AUTHENTIC;
	} else {
		peer->flags &= ~FLAG_AUTHENTIC;
	}
	if (peer->hmode == MODE_BROADCAST &&
	    (restrict_mask & RES_DONTTRUST))	/* test 4 */
		peer->flash |= TEST4;		/* access denied */
	if (peer->flags & FLAG_AUTHENABLE) {
		if (!(peer->flags & FLAG_AUTHENTIC)) /* test 5 */
			peer->flash |= TEST5;	/* auth failed */
		else if (!(oflags & FLAG_AUTHENABLE))
			report_event(EVNT_PEERAUTH, peer);
	}
	if (peer->flash) {
#ifdef DEBUG
		if (debug)
			printf("receive: bad auth %03x\n", peer->flash);
#endif
		return;
	}

#ifdef AUTOKEY
	/*
	 * More autokey dance. The rules of the cha-cha are as follows:
	 *
	 * 1. If there is no key or the key is not auto, do nothing.
	 *
	 * 2. If an extension field contains a verified signature, it is
	 *    self-authenticated and we sit the dance.
	 *
	 * 3. If this is a server reply, check only to see that the
	 *    transmitted key ID matches the received key ID.
	 *
	 * 4. Check to see that one or more hashes of the current key ID
	 *    matches the previous key ID or ultimate original key ID
	 *    obtained from the broadcaster or symmetric peer. If no
	 *    match, sit the dance and wait for timeout.
	 */
	if (peer->flags & FLAG_SKEY) {
		peer->flash |= TEST10;
		crypto_recv(peer, rbufp);
		poll_update(peer, peer->hpoll);
		if (hismode == MODE_SERVER) {
			if (skeyid == peer->keyid)
				peer->flash &= ~TEST10;
		} else if (!peer->flash & TEST10) {
			peer->pkeyid = skeyid;
		} else {
			int i;

			for (i = 0; ; i++) {
				if (tkeyid == peer->pkeyid ||
				    tkeyid == peer->recauto.key) {
					peer->flash &= ~TEST10;
					peer->pkeyid = skeyid;
					break;
				}
				if (i > peer->recauto.seq)
					break;
				tkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin,
				    tkeyid, pkeyid, 0);
			}
		}
#ifdef PUBKEY

		/*
		 * This is delicious. Ordinarily, we kick out all errors
		 * at this point; however, in symmetric mode and just
		 * warming up, an unsynchronized peer must inject the
		 * timestamps, even if it fails further up the road. So,
		 * let the dude by here, but only if the jerk is not yet
		 * reachable. After that, he's on his own.
		 */
		if (!(peer->flags & FLAG_PROVEN))
			peer->flash |= TEST11;
		if (peer->flash && peer->reach) {
#ifdef DEBUG
			if (debug)
				printf("packet: bad autokey %03x\n",
				    peer->flash);
#endif
			return;
		}
#endif /* PUBKEY */
	}
#endif /* AUTOKEY */

	/*
	 * We have survived the gaunt. Forward to the packet routine. If
	 * a symmetric passive association has been mobilized and the
	 * association doesn't deserve to live, it will die in the
	 * transmit routine if not reachable after timeout.
	 */
	process_packet(peer, pkt, &rbufp->recv_time);
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
	l_fp *recv_ts
	)
{
	l_fp t10, t23;
	double p_offset, p_del, p_disp;
	double dtemp;
	l_fp p_rec, p_xmt, p_org, p_reftime;
	l_fp ci;
	int pmode, pleap, pstratum;

	/*
	 * Swap header fields and keep the books. The books amount to
	 * the receive timestamp and poll interval in the header. We
	 * need these even if there are other problems in order to crank
	 * up the state machine.
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

	/*
	 * Test for old, duplicate or unsynch packets (tests 1-3).
	 */
	peer->rec = *recv_ts;
	pmode = PKT_MODE(pkt->li_vn_mode);
	pleap = PKT_LEAP(pkt->li_vn_mode);
	pstratum = PKT_TO_STRATUM(pkt->stratum);
	if (L_ISHIS(&peer->org, &p_xmt))	/* count old packets */
		peer->oldpkt++;
	if (L_ISEQU(&peer->org, &p_xmt))	/* 1 */
		peer->flash |= TEST1;		/* dupe */
	if (pmode != MODE_BROADCAST) {
		if (!L_ISEQU(&peer->xmt, &p_org)) /* 2 */
			peer->flash |= TEST2;	/* bogus */
		if (L_ISZERO(&p_rec) || L_ISZERO(&p_org)) /* test 3 */
			peer->flash |= TEST3;	/* unsynch */
	}
	if (L_ISZERO(&p_xmt))			/* 3 */
		peer->flash |= TEST3;		/* unsynch */
	peer->org = p_xmt;

	/*
	 * If tests 1-3 fail, the packet is discarded leaving only the
	 * receive and origin timestamps and poll interval, which is
	 * enough to get the protocol started.
	 */
	if (peer->flash) {
#ifdef DEBUG
		if (debug)
			printf("packet: bad data %03x\n",
			    peer->flash);
#endif
		return;
	}

	/*
	 * A kiss-of-death (kod) packet is returned by a server in case
	 * the client is denied access. It consists of the client
	 * request packet with the leap bits indicating never
	 * synchronized, stratum zero and reference ID field the ASCII
	 * string "DENY". If the packet originate timestamp matches the
	 * association transmit timestamp the kod is legitimate. If the
	 * peer leap bits indicate never synchronized, this must be
	 * access deny and the association is disabled; otherwise this
	 * must be a limit reject. In either case a naughty message is
	 * forced to the system log.
	 */
	if (pleap == LEAP_NOTINSYNC && pstratum >= STRATUM_UNSPEC &&
	    memcmp(&pkt->refid, "DENY", 4) == 0) {
		if (peer->leap == LEAP_NOTINSYNC) {
			peer->stratum = STRATUM_UNSPEC;
			peer->flash |= TEST4;
			memcpy(&peer->refid, &pkt->refid, 4);
			msyslog(LOG_INFO, "access denied");
		} else {
			msyslog(LOG_INFO, "limit reject");
		}
		return;
	}

	/*
	 * Test for valid peer data (tests 6-8)
	 */
	ci = p_xmt;
	L_SUB(&ci, &p_reftime);
	LFPTOD(&ci, dtemp);
	if (pleap == LEAP_NOTINSYNC ||		/* 6 */
	    pstratum >= STRATUM_UNSPEC || dtemp < 0)
		peer->flash |= TEST6;		/* bad synch */
	if (!(peer->flags & FLAG_CONFIG) && sys_peer != NULL) { /* 7 */
		if (pstratum > sys_stratum && pmode != MODE_ACTIVE) {
			peer->flash |= TEST7;	/* bad stratum */
			sys_badstratum++;
		}
	}
	if (p_del < 0 || p_disp < 0 || p_del /	/* 8 */
	    2 + p_disp >= MAXDISPERSE)
		peer->flash |= TEST8;		/* bad peer distance */
	if (peer->flash) {
#ifdef DEBUG
		if (debug)
			printf("packet: bad header %03x\n",
			    peer->flash);
#endif
		return;
	}

	/*
	 * The header is valid. Capture the remaining header values and
	 * mark as reachable.
	 */
	record_raw_stats(&peer->srcadr, &peer->dstadr->sin, &p_org,
	    &p_rec, &p_xmt, &peer->rec);
	peer->leap = pleap;
	peer->pmode = pmode;
	peer->stratum = pstratum;
	peer->ppoll = pkt->ppoll;
	peer->precision = pkt->precision;
	peer->rootdelay = p_del;
	peer->rootdispersion = p_disp;
	peer->refid = pkt->refid;
	peer->reftime = p_reftime;
	if (!(peer->reach)) {
		report_event(EVNT_REACH, peer);
		peer->timereachable = current_time;
	}
	peer->reach |= 1;
	peer->unreach = 0;
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
	p_disp = clock_phi * (peer->rec.l_ui - p_org.l_ui);

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
		if (peer->flags & FLAG_MCAST) {
			LFPTOD(&ci, p_offset);
			peer->estbdelay = peer->offset - p_offset;
			if (peer->hmode == MODE_CLIENT)
				return;

			peer->flags &= ~FLAG_MCAST;
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
	p_del = max(p_del, LOGTOD(sys_precision));
	LFPTOD(&ci, p_offset);
	if ((peer->rootdelay + p_del) / 2. + peer->rootdispersion +
	    p_disp >= MAXDISPERSE)		/* 9 */
		peer->flash |= TEST9;		/* bad peer distance */

	/*
	 * If any flasher bits remain set at this point, abandon ship.
	 * Otherwise, forward to the clock filter.
	 */
	if (peer->flash) {
#ifdef DEBUG
		if (debug)
			printf("packet: bad packet data %03x\n",
			    peer->flash);
#endif
		return;
	}
	clock_filter(peer, p_offset, p_del, p_disp);
	clock_select();
	record_peer_stats(&peer->srcadr, ctlpeerstatus(peer),
	    peer->offset, peer->delay, peer->disp,
	    SQRT(peer->jitter));
}


/*
 * clock_update - Called at system process update intervals.
 */
static void
clock_update(void)
{
	u_char oleap;
	u_char ostratum;

	/*
	 * Reset/adjust the system clock. Do this only if there is a
	 * system peer and we haven't seen that peer lately. Watch for
	 * timewarps here.
	 */
	if (sys_peer == NULL)
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
	switch (local_clock(sys_peer, sys_offset, sys_syserr)) {

	/*
	 * Clock is too screwed up. Just exit for now.
	 */
	case -1:
		report_event(EVNT_SYSFAULT, (struct peer *)0);
		exit(1);
		/*NOTREACHED*/

	/*
	 * Clock was stepped. Flush all time values of all peers.
	 */
	case 1:
		clear_all();
		sys_peer = NULL;
		sys_stratum = STRATUM_UNSPEC;
		sys_poll = NTP_MINPOLL;
		NLOG(NLOG_SYNCSTATUS)
		    msyslog(LOG_INFO, "synchronisation lost");
		report_event(EVNT_CLOCKRESET, (struct peer *)0);
		break;

	/*
	 * Update the system stratum, leap bits, root delay, root
	 * dispersion, reference ID and reference time. We also update
	 * select dispersion and max frequency error. If the leap
	 * changes, we gotta reroll the keys.
	 */
	default:
		sys_stratum = sys_peer->stratum + 1;
		if (sys_stratum == 1)
			sys_refid = sys_peer->refid;
		else
			sys_refid = sys_peer->srcadr.sin_addr.s_addr;
		sys_reftime = sys_peer->rec;
		sys_rootdelay = sys_peer->rootdelay + sys_peer->delay;
		sys_leap = leap_consensus;
	}
	if (oleap == LEAP_NOTINSYNC) {
		report_event(EVNT_SYNCCHG, (struct peer *)0);
#ifdef AUTOKEY
		expire_all();
#endif /* AUTOKEY */
	}
	if (ostratum != sys_stratum)
		report_event(EVNT_PEERSTCHG, (struct peer *)0);
}


/*
 * poll_update - update peer poll interval
 */
void
poll_update(
	struct peer *peer,
	int hpoll
	)
{
#ifdef AUTOKEY
	int oldpoll;
#endif /* AUTOKEY */

	/*
	 * A little foxtrot to determine what controls the poll
	 * interval. If the peer is reachable, but the last four polls
	 * have not been answered, use the minimum. If declared
	 * truechimer, use the system poll interval. This allows each
	 * association to ramp up the poll interval for useless sources
	 * and to clamp it to the minimum when first starting up.
	 */
#ifdef AUTOKEY
	oldpoll = peer->kpoll;
#endif /* AUTOKEY */
	if (hpoll > peer->maxpoll)
		peer->hpoll = peer->maxpoll;
	else if (hpoll < peer->minpoll)
		peer->hpoll = peer->minpoll;
	else
		peer->hpoll = hpoll;

	/*
	 * Bit of adventure here. If during a burst and not timeout,
	 * just slink away. If timeout, figure what the next timeout
	 * should be. If IBURST or a reference clock, use one second. If
	 * not and the dude was reachable during the previous poll
	 * interval, randomize over 1-4 seconds; otherwise, randomize
	 * over 15-18 seconds. This is to give time for a modem to
	 * complete the call, for example. If not during a burst,
	 * randomize over the poll interval -1 to +2 seconds.
	 *
	 * In case of manycast server, make the poll interval, which is
	 * axtually the manycast beacon interval, eight times the system
	 * poll interval. Normally when the host poll interval settles
	 * up to 17.1 s, the beacon interval settles up to 2.3 hours.
	 */
	if (peer->burst > 0) {
		if (peer->nextdate != current_time)
			return;
#ifdef REFCLOCK
		else if (peer->flags & FLAG_REFCLOCK)
			peer->nextdate++;
#endif
		else if (peer->reach & 0x1)
			peer->nextdate += RANDPOLL(BURST_INTERVAL2);
		else
			peer->nextdate += RANDPOLL(BURST_INTERVAL1);
	} else if (peer->cast_flags & MDF_ACAST) {
		if (sys_survivors < NTP_MINCLOCK)
			peer->kpoll = peer->hpoll;
		else
			peer->kpoll = peer->hpoll + 3;
		peer->nextdate = peer->outdate + RANDPOLL(peer->kpoll);
	} else {
		peer->kpoll = max(min(peer->ppoll, peer->hpoll),
		    peer->minpoll);
		peer->nextdate = peer->outdate + RANDPOLL(peer->kpoll);
	}
	if (peer->nextdate < current_time)
		peer->nextdate = current_time;
#ifdef AUTOKEY
	/*
	 * Bit of crass arrogance at this point. If the poll interval
	 * has changed and we have a keylist, the lifetimes in the
	 * keylist are probably bogus. In this case purge the keylist
	 * and regenerate it later.
	 */
	if (peer->kpoll != oldpoll)
		key_expire(peer);
#endif /* AUTOKEY */
#ifdef DEBUG
	if (debug > 1)
		printf("poll_update: at %lu %s flags %04x poll %d burst %d last %lu next %lu\n",
		    current_time, ntoa(&peer->srcadr), peer->flags,
		    peer->kpoll, peer->burst, peer->outdate,
		    peer->nextdate);
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
	u_long u_rand;

	/*
	 * If cryptographic credentials have been acquired, toss them to
	 * Valhalla. Note that autokeys are ephemeral, in that they are
	 * tossed immediately upon use. Therefore, the keylist can be
	 * purged anytime without needing to preserve random keys. Note
	 * that, if the peer is purged, the cryptographic variables are
	 * purged, too. This makes it much harder to sneak in some
	 * unauthenticated data in the clock filter.
	 */
#ifdef DEBUG
	if (debug)
		printf("peer_clear: at %ld assoc ID %d\n", current_time,
		    peer->associd);
#endif
#ifdef AUTOKEY
	key_expire(peer);
#ifdef PUBKEY
	if (peer->keystr != NULL)
		free(peer->keystr);
	if (peer->pubkey.ptr != NULL)
		free(peer->pubkey.ptr);
	if (peer->certif.ptr != NULL)
		free(peer->certif.ptr);
#endif /* PUBKEY */
#endif /* AUTOKEY */
	memset(CLEAR_TO_ZERO(peer), 0, LEN_CLEAR_TO_ZERO);

	/*
	 * If he dies as a broadcast client, he comes back to life as
	 * a broadcast client in client mode in order to recover the
	 * initial autokey values. Note that there is no need to call
	 * clock_select(), since the perp has already been voted off
	 * the island at this point.
	 */
	if (peer->cast_flags & MDF_BCLNT) {
		peer->flags |= FLAG_MCAST;
		peer->hmode = MODE_CLIENT;
	}
	peer->flags &= ~(FLAG_AUTOKEY | FLAG_ASSOC);
	peer->estbdelay = sys_bdelay;
	peer->hpoll = peer->kpoll = peer->minpoll;
	peer->ppoll = peer->maxpoll;
	peer->pollsw = FALSE;
	peer->jitter = MAXDISPERSE;
	peer->epoch = current_time;
#ifdef REFCLOCK
	if (!(peer->flags & FLAG_REFCLOCK)) {
		peer->leap = LEAP_NOTINSYNC;
		peer->stratum = STRATUM_UNSPEC;
	}
#endif
	for (i = 0; i < NTP_SHIFT; i++) {
		peer->filter_order[i] = i;
		peer->filter_disp[i] = MAXDISPERSE;
		peer->filter_epoch[i] = current_time;
	}

	/*
	 * Randomize the first poll over 1-16s to avoid bunching.
	 */
	peer->update = peer->outdate = current_time;
	u_rand = RANDOM;
	peer->nextdate = current_time + (u_rand & ((1 <<
	    BURST_INTERVAL1) - 1)) + 1;
}


/*
 * clock_filter - add incoming clock sample to filter register and run
 *		  the filter procedure to find the best sample.
 */
void
clock_filter(
	register struct peer *peer,	/* peer structure pointer */
	double sample_offset,		/* clock offset */
	double sample_delay,		/* roundtrip delay */
	double sample_disp		/* dispersion */
	)
{
	double dst[NTP_SHIFT];		/* distance vector */
	int ord[NTP_SHIFT];		/* index vector */
	register int i, j, k, m;
	double dsp, jit, dtemp, etemp;

	/*
	 * Shift the new sample into the register and discard the oldest
	 * one. The new offset and delay come directly from the
	 * timestamp calculations. The dispersion grows from the last
	 * outbound packet or reference clock update to the present time
	 * and increased by the sum of the peer precision and the system
	 * precision. The delay can sometimes swing negative due to
	 * frequency skew, so it is clamped non-negative.
	 */
	dsp = min(LOGTOD(peer->precision) + LOGTOD(sys_precision) +
	    sample_disp, MAXDISPERSE);
	j = peer->filter_nextpt;
	peer->filter_offset[j] = sample_offset;
	peer->filter_delay[j] = max(0, sample_delay);
	peer->filter_disp[j] = dsp;
	peer->filter_epoch[j] = current_time;
	j++; j %=NTP_SHIFT;
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
		if (i != 0) {
			peer->filter_disp[j] += dtemp;
			if (peer->filter_disp[j] > MAXDISPERSE)
				peer->filter_disp[j] = MAXDISPERSE;
		}
		if (peer->filter_disp[j] >= MAXDISPERSE)
			dst[i] = MAXDISPERSE;
		else if (peer->update - peer->filter_epoch[j] >
		    allan_xpt)
			dst[i] = MAXDISTANCE + peer->filter_disp[j];
		else
 			dst[i] = peer->filter_delay[j];
		ord[i] = j;
		j++; j %= NTP_SHIFT;
	}

        /*
	 * Sort the samples in both lists by distance.
	 */
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

	/*
	 * Copy the index list to the association structure so ntpq
	 * can see it later. Prune the distance list to samples less
	 * than MAXDISTANCE, but keep at least two valid samples for
	 * jitter calculation.
	 */
	m = 0;
	for (i = 0; i < NTP_SHIFT; i++) {
		peer->filter_order[i] = ord[i];
		if (dst[i] >= MAXDISPERSE || (m >= 2 && dst[i] >=
		    MAXDISTANCE))
			continue;
		m++;
	}
	
	/*
	 * Compute the dispersion and jitter squares. The dispersion
	 * is weighted exponentially by NTP_FWEIGHT (0.5) so it is
	 * normalized close to 1.0. The jitter is the mean of the square
	 * differences relative to the lowest delay sample. If no
	 * acceptable samples remain in the shift register, quietly
	 * tiptoe home leaving only the
	 * dispersion.
	 */
	jit = 0;
	peer->disp = 0;
	k = ord[0];
	for (i = NTP_SHIFT - 1; i >= 0; i--) {

		j = ord[i];
		peer->disp = NTP_FWEIGHT * (peer->disp +
		    peer->filter_disp[j]);
		if (i < m)
			jit += DIFF(peer->filter_offset[j],
			    peer->filter_offset[k]);
	}

	/*
	 * If no acceptable samples remain in the shift register,
	 * quietly tiptoe home leaving only the dispersion. Otherwise,
	 * save the offset, delay and jitter average. Note the jitter
	 * must not be less than the system precision.
	 */
	if (m == 0)
		return;
	etemp = peer->offset;
	peer->offset = peer->filter_offset[k];
	peer->delay = peer->filter_delay[k];
	if (m > 1)
		jit /= m - 1;
	peer->jitter = max(jit, SQUARE(LOGTOD(sys_precision)));

	/*
	 * A new sample is useful only if it is younger than the last
	 * one used.
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
	 * exceeds the jitter by CLOCK_SGATE (4) and the interval since
	 * the last update is less than twice the system poll interval,
	 * consider the update a popcorn spike and ignore it.
	 */
	if (m > 1 && fabs(peer->offset - etemp) > SQRT(peer->jitter) *
	    CLOCK_SGATE && peer->filter_epoch[k] - peer->epoch <
	    (1 << (sys_poll + 1))) {
#ifdef DEBUG
		if (debug)
			printf("clock_filter: n %d popcorn spike %.6f jitter %.6f\n",
			    m, peer->offset, SQRT(peer->jitter));
#endif
		return;
	}

	/*
	 * The mitigated sample statistics are saved for later
	 * processing, but can be processed only once.
	 */
	peer->epoch = peer->filter_epoch[k];
	peer->pollsw = TRUE;
#ifdef DEBUG
	if (debug)
		printf(
		    "clock_filter: n %d off %.6f del %.6f dsp %.6f jit %.6f, age %lu\n",
		    m, peer->offset, peer->delay, peer->disp,
		    SQRT(peer->jitter), peer->update - peer->epoch);
#endif
}


/*
 * clock_select - find the pick-of-the-litter clock
 */
void
clock_select(void)
{
	register struct peer *peer;
	int i, j, k, n;
	int nreach, nlist, nl3;
	double d, e, f;
	int allow, found, sw;
	double high, low;
	double synch[NTP_MAXCLOCK], error[NTP_MAXCLOCK];
	struct peer *osys_peer;
	struct peer *typeacts = NULL;
	struct peer *typelocal = NULL;
	struct peer *typepps = NULL;
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
	sys_prefer = NULL;
	nreach = nlist = 0;
	low = 1e9;
	high = -1e9;
	for (n = 0; n < HASH_SIZE; n++)
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
	 * has dwindled to NTP_MINCLOCK (3), the survivors split a
	 * million bucks and collectively crank the chimes.
	 */
	nlist = nl3 = 0;	/* none yet */
	for (n = 0; n < HASH_SIZE; n++) {
		for (peer = peer_hash[n]; peer != NULL; peer =
		    peer->next) {
			peer->flags &= ~FLAG_SYSPEER;
			peer->status = CTL_PST_SEL_REJECT;

			/*
			 * A peer leaves the island immediately if
			 * unreachable, synchronized to us or suffers
			 * excessive root distance. Careful with the
			 * root distance, since the poll interval can
			 * increase to a day and a half.
			 */ 
			if (!peer->reach || (peer->stratum > 1 &&
			    peer->refid ==
			    peer->dstadr->sin.sin_addr.s_addr) ||
			    peer->stratum >= STRATUM_UNSPEC ||
			    (root_distance(peer) >= MAXDISTANCE + 2 *
			    clock_phi * ULOGTOD(sys_poll)))
				continue;

			/*
			 * Don't allow the local clock or modem drivers
			 * in the kitchen at this point, unless the
			 * prefer peer. Do that later, but only if
			 * nobody else is around. These guys are all
			 * configured, so we never throw them away.
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
			 * If we get this far, the peer can stay on the
			 * island, but does not yet have the immunity
			 * idol.
			 */
			nreach++;
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
			for ( ; i >= 0; i--) {
				if (e >= endpoint[indx[i]].val)
					break;
				indx[i + 2] = indx[i];
			}
			indx[i + 2] = nl3;
			endpoint[nl3].type = 0;
			endpoint[nl3++].val = e;

			e = e - f;		/* Lower end */
			for ( ; i >= 0; i--) {
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
	i = 0;
	j = nl3 - 1;
	allow = nlist;		/* falsetickers assumed */
	found = 0;
	while (allow > 0) {
		allow--;
		for (n = 0; i <= j; i++) {
			n += endpoint[indx[i]].type;
			if (n < 0)
				break;
			if (endpoint[indx[i]].type == 0)
				found++;
		}
		for (n = 0; i <= j; j--) {
			n += endpoint[indx[j]].type;
			if (n > 0)
				break;
			if (endpoint[indx[j]].type == 0)
				found++;
		}
		if (found > allow)
			break;
		low = endpoint[indx[i++]].val;
		high = endpoint[indx[j--]].val;
	}

	/*
	 * If no survivors remain at this point, check if the local
	 * clock or modem drivers have been found. If so, nominate one
	 * of them as the only survivor. Otherwise, give up and declare
	 * us unsynchronized.
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
			if (osys_peer != NULL) {
				sys_poll = NTP_MINPOLL;
				NLOG(NLOG_SYNCSTATUS)
				    msyslog(LOG_INFO,
				    "synchronisation lost");
				report_event(EVNT_PEERSTCHG,
				    (struct peer *)0);
			}
			sys_survivors = 0;
#ifdef AUTOKEY
			resetmanycast();
#endif /* AUTOKEY */
			return;
		}
	}
#ifdef DEBUG
	if (debug > 2)
		printf("select: low %.6f high %.6f\n", low, high);
#endif

	/*
	 * Clustering algorithm. Construct candidate list in order first
	 * by stratum then by root distance. If we have more than
	 * MAXCLOCK peers, keep only the best MAXCLOCK of them. Scan the
	 * list to find falsetickers, who leave the island immediately.
	 * If a falseticker is not configured, his association raft is
	 * drowned as well. We must leave at least one peer to collect
	 * the million bucks.
	 */
	j = 0;
	for (i = 0; i < nlist; i++) {
		peer = peer_list[i];
		if (nlist > 1 && (low >= peer->offset || peer->offset >=
		    high)) {
			if (!(peer->flags & FLAG_CONFIG))
				unpeer(peer);
			continue;
		}
		peer->status = CTL_PST_SEL_DISTSYSPEER;
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
	for (i = 0; i < nlist; i++) {
		peer_list[i]->status = CTL_PST_SEL_SELCAND;

#ifdef DEBUG
		if (debug > 2)
			printf("select: %s distance %.6f\n",
			    ntoa(&peer_list[i]->srcadr), synch[i]);
#endif
	}

	/*
	 * Now, vote outlyers off the island by select jitter weighted
	 * by root dispersion. Continue voting as long as there are more
	 * than NTP_MINCLOCK survivors and the minimum select jitter
	 * squared is greater than the maximum peer jitter squared. Stop
	 * if we are about to discard a prefer peer, who of course has
	 * the immunity idol.
	 */
	while (1) {
		d = 1e9;
		e = -1e9;
		k = 0;
		for (i = 0; i < nlist; i++) {

			if (error[i] < d)
				d = error[i];
			f = 0;
			if (nlist > 1) {
				for (j = 0; j < nlist; j++)
					f += DIFF(peer_list[j]->offset,
					    peer_list[i]->offset);
				f /= nlist - 1;
			}
			f = max(f, SQUARE(LOGTOD(sys_precision))); 
			if (f * synch[i] > e) {
				sys_selerr = f;
				e = f * synch[i];
				k = i;
			}
		}

#ifdef DEBUG
		if (debug > 2)
			printf(
			    "select: survivors %d select %.6f peer %.6f\n",
			    k, SQRT(sys_selerr), SQRT(d));
#endif
		if (nlist <= NTP_MINCLOCK || sys_selerr <= d ||
		    peer_list[k]->flags & FLAG_PREFER)
			break;
		if (!(peer_list[k]->flags & FLAG_CONFIG))
			unpeer(peer_list[k]);
		for (j = k + 1; j < nlist; j++) {
			peer_list[j - 1] = peer_list[j];
			error[j - 1] = error[j];
		}
		nlist--;
	}

#ifdef AUTOKEY
	/*
	 * In manycast client mode we may have spooked a sizeable number
	 * of servers that we don't need. If there are at least
	 * NTP_MINCLOCK of them, the manycast message will be turned
	 * off. By the time we get here we nay be ready to prune some of
	 * them back, but we want to make sure all the candicates have
	 * had a chance. If they didn't pass the sanity and intersection
	 * tests, they have already been voted off the island.
	 */
	if (sys_survivors >= NTP_MINCLOCK && nlist < NTP_MINCLOCK)
		resetmanycast();
#endif /* AUTOKEY */
	sys_survivors = nlist;

#ifdef DEBUG
	if (debug > 2) {
		for (i = 0; i < nlist; i++)
			printf(
			    "select: %s offset %.6f, distance %.6f poll %d\n",
			    ntoa(&peer_list[i]->srcadr),
			    peer_list[i]->offset, synch[i],
			    peer_list[i]->pollsw);
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
	 *
	 * Note that we go no further, unless the number of survivors is
	 * a majority of the suckers that have been found reachable and
	 * no prior source is available. This avoids the transient when
	 * one of a flock of sources is out to lunch and just happens
	 * to be the first survivor.
	 */
	if (osys_peer == NULL && 2 * nlist < min(nreach, NTP_MINCLOCK))
		return;
	leap_consensus = 0;
	for (i = nlist - 1; i >= 0; i--) {
		peer = peer_list[i];
		peer->status = CTL_PST_SEL_SYNCCAND;
		peer->flags |= FLAG_SYSPEER;
		poll_update(peer, peer->hpoll);
		if (peer->stratum == peer_list[0]->stratum) {
			leap_consensus |= peer->leap;
			if (peer->refclktype == REFCLK_ATOM_PPS &&
			    peer->stratum < STRATUM_UNSPEC)
				typepps = peer;
			if (peer == osys_peer)
				typesystem = peer;
			if (peer->flags & FLAG_PREFER)
				sys_prefer = peer;
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
	if (sys_prefer)
		sw = sys_prefer->refclktype == REFCLK_LOCALCLOCK ||
		    sys_prefer->sstclktype == CTL_SST_TS_TELEPHONE ||
		    !typepps;
	else
		sw = 0;
	if (sw) {
		sys_peer = sys_prefer;
		sys_peer->status = CTL_PST_SEL_SYSPEER;
		sys_offset = sys_peer->offset;
		sys_syserr = sys_peer->jitter;
#ifdef DEBUG
		if (debug > 1)
			printf("select: prefer offset %.6f\n",
			    sys_offset);
#endif
	} else if (typepps) {
		sys_peer = typepps;
		sys_peer->status = CTL_PST_SEL_PPS;
		sys_offset = sys_peer->offset;
		sys_syserr = sys_peer->jitter;
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
		if (typesystem)
			sys_peer = osys_peer;
		else
			sys_peer = peer_list[0];
		sys_peer->status = CTL_PST_SEL_SYSPEER;
		sys_offset = clock_combine(peer_list, nlist);
		sys_syserr = sys_peer->jitter + sys_selerr;
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
	/*
	 * Careful squeak here. The value returned must be greater than
	 * zero blamed on the peer jitter, which must be at least the
	 * square of sys_precision.
	 */
	return ((peer->rootdelay + peer->delay) / 2 +
	    peer->rootdispersion + peer->disp + clock_phi *
	    (current_time - peer->update) + SQRT(peer->jitter));
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
	int sendlen, authlen;
	keyid_t xkeyid;		/* transmit key ID */
	l_fp xmt_tx;

	/*
	 * Initialize transmit packet header fields.
	 */
	xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap, peer->version,
	    peer->hmode);
	xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
	xpkt.ppoll = peer->hpoll;
	xpkt.precision = sys_precision;
	xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
	xpkt.rootdispersion = HTONS_FP(DTOUFP(sys_rootdispersion));
	xpkt.refid = sys_refid;
	HTONL_FP(&sys_reftime, &xpkt.reftime);
	HTONL_FP(&peer->org, &xpkt.org);
	HTONL_FP(&peer->rec, &xpkt.rec);

	/*
	 * If the received packet contains a MAC, the transmitted packet
	 * is authenticated and contains a MAC. If not, the transmitted
	 * packet is not authenticated.
	 *
	 * In the current I/O semantics the default interface is set
	 * until after receiving a packet and setting the right
	 * interface. So, the first packet goes out unauthenticated.
	 * That's why the really icky test next is here.
	 */
	sendlen = LEN_PKT_NOMAC;
	if (!(peer->flags & FLAG_AUTHENABLE)) {
		get_systime(&peer->xmt);
		HTONL_FP(&peer->xmt, &xpkt.xmt);
		sendpkt(&peer->srcadr, peer->dstadr, peer->ttl, &xpkt,
		    sendlen);
		peer->sent++;
#ifdef DEBUG
		if (debug)
			printf("transmit: at %ld %s->%s mode %d\n",
			    current_time, ntoa(&peer->dstadr->sin),
			    ntoa(&peer->srcadr), peer->hmode);
#endif
		return;
	}

	/*
	 * The received packet contains a MAC, so the transmitted packet
	 * must be authenticated. If autokey is enabled, fuss with the
	 * various modes; otherwise, private key cryptography is used.
	 */
#ifdef AUTOKEY
	if ((peer->flags & FLAG_SKEY)) {
		u_int cmmd;

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
		switch (peer->hmode) {

		/*
		 * In broadcast mode the autokey values are required.
		 * Send them when a new keylist is generated; otherwise,
		 * send the association ID so the client can request
		 * them at other times.
		 */
		case MODE_BROADCAST:
			if (peer->flags & FLAG_ASSOC)
				cmmd = CRYPTO_AUTO | CRYPTO_RESP;
			else
				cmmd = CRYPTO_ASSOC | CRYPTO_RESP;
			sendlen += crypto_xmit((u_int32 *)&xpkt,
			    sendlen, cmmd, 0, peer->associd);
			break;

		/*
		 * In symmetric modes the public key, leapsecond table,
		 * agreement parameters and autokey values are required.
 		 *
		 * 1. If a response is pending, always send it first.
		 *
		 * 2. Don't send anything except a public-key request
		 *    until the public key has been stored. 
		 *
		 * 3. Once the public key has been stored, don't send
		 *    anything except an agreement parameter request
		 *    until the agreement parameters have been stored.
		 *
		 * 4. Once the argeement parameters have been stored,
		 *    don't send anything except a public value request
		 *    until the agreed key has been stored.
		 *
		 * 5. When the agreed key has been stored and the key
		 *    list is regenerated, send the autokey values
		 *    gratis unless they have already been sent.
		 */
		case MODE_ACTIVE:
		case MODE_PASSIVE:
#ifdef PUBKEY
			if (peer->cmmd != 0)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, (peer->cmmd >> 16) |
				    CRYPTO_RESP, peer->hcookie,
				    peer->associd);
			if (!peer->crypto)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_ASSOC,
				    peer->hcookie, peer->assoc);
			else if (!crypto_flags &&
			    peer->pcookie.tstamp == 0 && sys_leap !=
			    LEAP_NOTINSYNC)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_PRIV, peer->hcookie,
				    peer->assoc);
			else if (crypto_flags && peer->pubkey.ptr ==
			    NULL)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_NAME, peer->hcookie,
				    peer->assoc);
			else if (peer->crypto & CRYPTO_FLAG_CERT)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_CERT, peer->hcookie,
				    peer->assoc);
			else if (crypto_flags && peer->crypto &
			    CRYPTO_FLAG_DH && sys_leap !=
			    LEAP_NOTINSYNC)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_DHPAR,
				    peer->hcookie, peer->assoc);
			else if (crypto_flags && peer->pcookie.tstamp ==
			    0 && sys_leap != LEAP_NOTINSYNC)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_DH, peer->hcookie,
				    peer->assoc);
#else
			if (peer->cmmd != 0)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, (peer->cmmd >> 16) |
				    CRYPTO_RESP, peer->hcookie,
				    peer->associd);
			if (peer->pcookie.tstamp == 0 && sys_leap !=
			    LEAP_NOTINSYNC)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_PRIV, peer->hcookie,
				    peer->assoc);
#endif /* PUBKEY */
			else if (!(peer->flags & FLAG_AUTOKEY))
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_AUTO, peer->hcookie,
				    peer->assoc);
			else if ((peer->flags & FLAG_ASSOC) &&
			    (peer->cmmd >> 16) != CRYPTO_AUTO)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_AUTO | CRYPTO_RESP,
				    peer->hcookie, peer->associd);
#ifdef PUBKEY
			else if (peer->crypto & CRYPTO_FLAG_TAI)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_TAI, peer->hcookie,
				    peer->assoc);
#endif /* PUBKEY */
			peer->cmmd = 0;
			break;

		/*
		 * In client mode, the public key, host cookie and
		 * autokey values are required. In broadcast client
		 * mode, these values must be acquired during the
		 * client/server exchange to avoid having to wait until
		 * the next key list regeneration. Otherwise, the poor
		 * dude may die a lingering death until becoming
		 * unreachable and attempting rebirth. Note that we ask
		 * for the cookie at each key list regeneration anyway.
		 */
		case MODE_CLIENT:
			if (peer->cmmd != 0)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, (peer->cmmd >> 16) |
				    CRYPTO_RESP, peer->hcookie,
				    peer->associd);
			if (!peer->crypto)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_ASSOC,
				    peer->hcookie, peer->assoc);
#ifdef PUBKEY
			else if (crypto_flags && peer->pubkey.ptr ==
			    NULL)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_NAME, peer->hcookie,
				    peer->assoc);
			else if (peer->crypto & CRYPTO_FLAG_CERT)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_CERT, peer->hcookie,
				    peer->assoc);
#endif /* PUBKEY */
			else if (peer->pcookie.tstamp == 0)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_PRIV, peer->hcookie,
				    peer->assoc);
			else if (!(peer->flags & FLAG_AUTOKEY) &&
			    (peer->cast_flags & MDF_BCLNT))
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_AUTO, peer->hcookie,
				    peer->assoc);
#ifdef PUBKEY
			else if (peer->crypto & CRYPTO_FLAG_TAI)
				sendlen += crypto_xmit((u_int32 *)&xpkt,
				    sendlen, CRYPTO_TAI, peer->hcookie,
				    peer->assoc);
#endif /* PUBKEY */
			peer->cmmd = 0;
			break;
		}

		/*
		 * If extension fields are present, we must use a
		 * private value of zero and force min poll interval.  
		 * Most intricate.
		 */
		if (sendlen > LEN_PKT_NOMAC)
			session_key(&peer->dstadr->sin, &peer->srcadr,
			    xkeyid, 0, 2);
	} 
#endif /* AUTOKEY */
	xkeyid = peer->keyid;
	get_systime(&peer->xmt);
	L_ADD(&peer->xmt, &sys_authdelay);
	HTONL_FP(&peer->xmt, &xpkt.xmt);
	authlen = authencrypt(xkeyid, (u_int32 *)&xpkt, sendlen);
	if (authlen == 0) {
		msyslog(LOG_NOTICE,
			"transmit: no encryption key found");
		peer->flash |= TEST4 | TEST5;
		return;
	}
	sendlen += authlen;
#ifdef AUTOKEY
	if (xkeyid > NTP_MAXKEY)
		authtrust(xkeyid, 0);
#endif /* AUTOKEY */
	get_systime(&xmt_tx);
	if (sendlen > sizeof(xpkt)) {
		msyslog(LOG_ERR, "buffer overflow %u", sendlen);
		exit(-1);
	}
	sendpkt(&peer->srcadr, peer->dstadr, peer->ttl, &xpkt, sendlen);

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
#ifdef AUTOKEY
#ifdef DEBUG
	if (debug)
		printf(
		    "transmit: at %ld %s->%s mode %d keyid %08x len %d mac %d index %d\n",
		    current_time, ntoa(&peer->dstadr->sin),
		    ntoa(&peer->srcadr), peer->hmode, xkeyid, sendlen,
		    authlen, peer->keynumber);
#endif
#else
#ifdef DEBUG
	if (debug)
		printf(
		    "transmit: at %ld %s->%s mode %d keyid %08x len %d mac %d\n",
		    current_time, ntoa(&peer->dstadr->sin),
		    ntoa(&peer->srcadr), peer->hmode, xkeyid, sendlen,
		    authlen);
#endif
#endif /* AUTOKEY */
}


/*
 * fast_xmit - Send packet for nonpersistent association. Note that
 * neither the source or destination can be a broadcast address.
 */
static void
fast_xmit(
	struct recvbuf *rbufp,	/* receive packet pointer */
	int xmode,		/* transmit mode */
	keyid_t xkeyid,		/* transmit key ID */
	int mask		/* restrict mask */
	)
{
	struct pkt xpkt;	/* transmit packet structure */
	struct pkt *rpkt;	/* receive packet structure */
	l_fp xmt_ts;		/* transmit timestamp */
	l_fp xmt_tx;		/* transmit timestamp after authent */
	int sendlen, authlen;

	/*
	 * Initialize transmit packet header fields from the receive
	 * buffer provided. We leave some fields intact as received. If
	 * the gazinta was from a multicast address, the gazouta must go
	 * out another way.
	 */
	rpkt = &rbufp->recv_pkt;
	if (rbufp->dstadr->flags & INT_MULTICAST)
		rbufp->dstadr = findinterface(&rbufp->recv_srcadr);

	/*
	 * If the caller is restricted, return a kiss-of-death packet;
	 * otherwise, smooch politely.
	 */
	if (mask & (RES_DONTSERVE | RES_LIMITED)) {
		if (!(mask & RES_DEMOBILIZE)) {
			return;
		} else {
			xpkt.li_vn_mode =
			    PKT_LI_VN_MODE(LEAP_NOTINSYNC,
			    PKT_VERSION(rpkt->li_vn_mode), xmode);
			xpkt.stratum = STRATUM_UNSPEC;
			memcpy(&xpkt.refid, "DENY", 4);
		}
	} else {
		xpkt.li_vn_mode = PKT_LI_VN_MODE(sys_leap,
		    PKT_VERSION(rpkt->li_vn_mode), xmode);
		xpkt.stratum = STRATUM_TO_PKT(sys_stratum);
		xpkt.refid = sys_refid;
	}
	xpkt.ppoll = rpkt->ppoll;
	xpkt.precision = sys_precision;
	xpkt.rootdelay = HTONS_FP(DTOFP(sys_rootdelay));
	xpkt.rootdispersion =
	    HTONS_FP(DTOUFP(sys_rootdispersion));
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
			    current_time, ntoa(&rbufp->dstadr->sin),
			    ntoa(&rbufp->recv_srcadr), xmode);
#endif
		return;
	}

	/*
	 * The received packet contains a MAC, so the transmitted packet
	 * must be authenticated. For private-key cryptography, use the
	 * predefined private keys to generate the cryptosum. For
	 * autokey cryptography, use the server private value to
	 * generate the cookie, which is unique for every source-
	 * destination-key ID combination.
	 */
#ifdef AUTOKEY
	if (xkeyid > NTP_MAXKEY) {
		keyid_t cookie;
		u_int code, associd;

		/*
		 * The only way to get here is a reply to a legitimate
		 * client request message, so the mode must be
		 * MODE_SERVER. If an extension field is present, there
		 * can be only one and that must be a command. Do what
		 * needs, but with private value of zero so the poor
		 * jerk can decode it. If no extension field is present,
		 * use the cookie to generate the session key.
		 */
		code = (htonl(rpkt->exten[0]) >> 16) | CRYPTO_RESP;
		cookie = session_key(&rbufp->recv_srcadr,
		    &rbufp->dstadr->sin, 0, sys_private, 0);
		associd = htonl(rpkt->exten[1]);
		if (rbufp->recv_length >= sendlen + MAX_MAC_LEN + 2 *
		    sizeof(u_int32)) {
			session_key(&rbufp->dstadr->sin,
			    &rbufp->recv_srcadr, xkeyid, 0, 2);
			sendlen += crypto_xmit((u_int32 *)&xpkt,
			    sendlen, code, cookie, associd);
		} else {
			session_key(&rbufp->dstadr->sin,
			    &rbufp->recv_srcadr, xkeyid, cookie, 2);
		}
	}
#endif /* AUTOKEY */
	get_systime(&xmt_ts);
	L_ADD(&xmt_ts, &sys_authdelay);
	HTONL_FP(&xmt_ts, &xpkt.xmt);
	authlen = authencrypt(xkeyid, (u_int32 *)&xpkt, sendlen);
	sendlen += authlen;
#ifdef AUTOKEY
	if (xkeyid > NTP_MAXKEY)
		authtrust(xkeyid, 0);
#endif /* AUTOKEY */
	get_systime(&xmt_tx);
	if (sendlen > sizeof(xpkt)) {
		msyslog(LOG_ERR, "buffer overflow %u", sendlen);
		exit(-1);
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
		    ntoa(&rbufp->recv_srcadr), xmode, xkeyid, sendlen,
		    authlen);
#endif
}


#ifdef AUTOKEY
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
	peer->keynumber = peer->sndauto.seq = 0;
#ifdef DEBUG
	if (debug)
		printf("key_expire: at %lu\n", current_time);
#endif
}
#endif /* AUTOKEY */

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
	sys_jitter = LOGTOD(sys_precision);
	sys_rootdelay = 0;
	sys_rootdispersion = 0;
	sys_refid = 0;
	L_CLR(&sys_reftime);
	sys_peer = NULL;
	sys_survivors = 0;
	get_systime(&dummy);
	sys_bclient = 0;
	sys_bdelay = DEFBROADDELAY;
	sys_authenticate = 1;
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
#ifdef AUTOKEY
	sys_automax = 1 << NTP_AUTOMAX;
#endif /* AUTOKEY */

	/*
	 * Default these to enable
	 */
	ntp_enable = 1;
#ifndef KERNEL_FLL_BUG
	kern_enable = 1;
#endif
	pps_enable = 0;
	stats_control = 1;

	/*
	 * Some system clocks should only be adjusted in 10ms
	 * increments.
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

	case PROTO_PPS:

		/*
		 * Turn on/off PPS discipline
		 */
		pps_enable = (int)value;
		break;

#ifdef REFCLOCK
	case PROTO_CAL:

		/*
		 * Turn on/off refclock calibrate
		 */
		cal_enable = (int)value;
		break;
#endif

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
