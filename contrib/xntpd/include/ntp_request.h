/* ntp_request.h,v 3.1 1993/07/06 01:06:57 jbj Exp
 * ntp_request.h - definitions for the xntpd remote query facility
 */

#include "ntp_types.h"

/*
 * A mode 7 packet is used exchanging data between an NTP server
 * and a client for purposes other than time synchronization, e.g.
 * monitoring, statistics gathering and configuration.  A mode 7
 * packet has the following format:
 *
 *    0			  1		      2			  3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |R|M| VN  | Mode|A|  Sequence   | Implementation|   Req Code    |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |  Err  | Number of data items  |  MBZ  |   Size of data item   |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |								     |
 *   |            Data (Minimum 0 octets, maximum 500 octets)        |
 *   |								     |
 *                            [...]
 *   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |               Encryption Keyid (when A bit set)               |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |								     |
 *   |          Message Authentication Code (when A bit set)         |
 *   |								     |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * where the fields are (note that the client sends requests, the server
 * responses):
 *
 * Response Bit:  This packet is a response (if clear, packet is a request).
 *
 * More Bit:	Set for all packets but the last in a response which
 *		requires more than one packet.
 *
 * Version Number: 2 for current version
 *
 * Mode:	Always 7
 *
 * Authenticated bit: If set, this packet is authenticated.
 *
 * Sequence number: For a multipacket response, contains the sequence
 *		number of this packet.  0 is the first in the sequence,
 *		127 (or less) is the last.  The More Bit must be set in
 *		all packets but the last.
 *
 * Implementation number: The number of the implementation this request code
 *		is defined by.  An implementation number of zero is used
 *		for requst codes/data formats which all implementations
 *		agree on.  Implementation number 255 is reserved (for
 *		extensions, in case we run out).
 *
 * Request code: An implementation-specific code which specifies the
 *		operation to be (which has been) performed and/or the
 *		format and semantics of the data included in the packet.
 *
 * Err:		Must be 0 for a request.  For a response, holds an error
 *		code relating to the request.  If nonzero, the operation
 *		requested wasn't performed.
 *
 *		0 - no error
 *		1 - incompatable implementation number
 *		2 - unimplemented request code
 *		3 - format error (wrong data items, data size, packet size etc.)
 *		4 - no data available (e.g. request for details on unknown peer)
 *		5-6 I don't know
 *		7 - authentication failure (i.e. permission denied)
 *
 * Number of data items: number of data items in packet.  0 to 500
 *
 * MBZ:		A reserved data field, must be zero in requests and responses.
 *
 * Size of data item: size of each data item in packet.  0 to 500
 *
 * Data:	Variable sized area containing request/response data.  For
 *		requests and responses the size in octets must be greater
 *		than or equal to the product of the number of data items
 *		and the size of a data item.  For requests the data area
 *		must be exactly 40 octets in length.  For responses the
 *		data area may be any length between 0 and 500 octets
 *		inclusive.
 *
 * Message Authentication Code: Same as NTP spec, in definition and function.
 *		May optionally be included in requests which require
 *		authentication, is never included in responses.
 *
 * The version number, mode and keyid have the same function and are
 * in the same location as a standard NTP packet.  The request packet
 * is the same size as a standard NTP packet to ease receive buffer
 * management, and to allow the same encryption procedure to be used
 * both on mode 7 and standard NTP packets.  The mac is included when
 * it is required that a request be authenticated, the keyid should be
 * zero in requests in which the mac is not included.
 *
 * The data format depends on the implementation number/request code pair
 * and whether the packet is a request or a response.  The only requirement
 * is that data items start in the octet immediately following the size
 * word and that data items be concatenated without padding between (i.e.
 * if the data area is larger than data_items*size, all padding is at
 * the end).  Padding is ignored, other than for encryption purposes.
 * Implementations using encryption might want to include a time stamp
 * or other data in the request packet padding.  The key used for requests
 * is implementation defined, but key 15 is suggested as a default.
 */

/*
 * A request packet.  These are almost a fixed length.
 */
struct req_pkt {
	u_char rm_vn_mode;		/* response, more, version, mode */
	u_char auth_seq;		/* key, sequence number */
	u_char implementation;		/* implementation number */
	u_char request;			/* request number */
	u_short err_nitems;		/* error code/number of data items */
	u_short mbz_itemsize;		/* item size */
	char data[32];			/* data area */
	l_fp tstamp;			/* time stamp, for authentication */
	U_LONG keyid;			/* encryption key */
	char mac[MAX_MAC_LEN-sizeof(U_LONG)]; /* (optional) 8 byte auth code */
};

/*
 * Input packet lengths.  One with the mac, one without.
 */
#define	REQ_LEN_MAC	(sizeof(struct req_pkt))
#define	REQ_LEN_NOMAC	(sizeof(struct req_pkt) - MAX_MAC_LEN)

/*
 * A response packet.  The length here is variable, this is a
 * maximally sized one.  Note that this implementation doesn't
 * authenticate responses.
 */
#define	RESP_HEADER_SIZE	(8)
#define	RESP_DATA_SIZE		(500)

struct resp_pkt {
	u_char rm_vn_mode;		/* response, more, version, mode */
	u_char auth_seq;		/* key, sequence number */
	u_char implementation;		/* implementation number */
	u_char request;			/* request number */
	u_short err_nitems;		/* error code/number of data items */
	u_short mbz_itemsize;		/* item size */
	char data[RESP_DATA_SIZE];	/* data area */
};


/*
 * Information error codes
 */
#define	INFO_OKAY	0
#define	INFO_ERR_IMPL	1	/* incompatable implementation */
#define	INFO_ERR_REQ	2	/* unknown request code */
#define	INFO_ERR_FMT	3	/* format error */
#define	INFO_ERR_NODATA	4	/* no data for this request */
#define	INFO_ERR_AUTH	7	/* authentication failure */

/*
 * Maximum sequence number.
 */
#define	MAXSEQ	127


/*
 * Bit setting macros for multifield items.
 */
#define	RESP_BIT	0x80
#define	MORE_BIT	0x40

#define	ISRESPONSE(rm_vn_mode)	(((rm_vn_mode)&RESP_BIT)!=0)
#define	ISMORE(rm_vn_mode)	(((rm_vn_mode)&MORE_BIT)!=0)
#define INFO_VERSION(rm_vn_mode) ((u_char)(((rm_vn_mode)>>3)&0x7))
#define	INFO_MODE(rm_vn_mode)	((rm_vn_mode)&0x7)

#define	RM_VN_MODE(resp, more)	((u_char)(((resp)?RESP_BIT:0)\
				|((more)?MORE_BIT:0)\
				|((NTP_VERSION)<<3)\
				|(MODE_PRIVATE)))

#define	INFO_IS_AUTH(auth_seq)	(((auth_seq) & 0x80) != 0)
#define	INFO_SEQ(auth_seq)	((auth_seq)&0x7f)
#define	AUTH_SEQ(auth, seq)	((u_char)((((auth)!=0)?0x80:0)|((seq)&0x7f)))

#define	INFO_ERR(err_nitems)	((u_short)((ntohs(err_nitems)>>12)&0xf))
#define	INFO_NITEMS(err_nitems)	((u_short)(ntohs(err_nitems)&0xfff))
#define	ERR_NITEMS(err, nitems)	(htons((((u_short)(err)<<12)&0xf000)\
				|((u_short)(nitems)&0xfff)))

#define	INFO_MBZ(mbz_itemsize)	((ntohs(mbz_itemsize)>>12)&0xf)
#define	INFO_ITEMSIZE(mbz_itemsize)	(ntohs(mbz_itemsize)&0xfff)
#define	MBZ_ITEMSIZE(itemsize)	(htons((u_short)(itemsize)))


/*
 * Implementation numbers.  One for universal use and one for xntpd.
 */
#define	IMPL_UNIV	0
#define	IMPL_XNTPD	2

/*
 * Some limits related to authentication.  Frames which are
 * authenticated must include a time stamp which differs from
 * the receive time stamp by no more than 10 seconds.
 */
#define	INFO_TS_MAXSKEW_UI	10
#define	INFO_TS_MAXSKEW_UF	0

/*
 * Universal request codes go here.  There aren't any.
 */

/*
 * XNTPD request codes go here.
 */
#define	REQ_PEER_LIST		0	/* return list of peers */
#define	REQ_PEER_LIST_SUM	1	/* return summary info for all peers */
#define	REQ_PEER_INFO		2	/* get standard information on peer */
#define	REQ_PEER_STATS		3	/* get statistics for peer */
#define	REQ_SYS_INFO		4	/* get system information */
#define	REQ_SYS_STATS		5	/* get system stats */
#define	REQ_IO_STATS		6	/* get I/O stats */
#define REQ_MEM_STATS		7	/* stats related to peer list maint */
#define	REQ_LOOP_INFO		8	/* info from the loop filter */
#define	REQ_TIMER_STATS		9	/* get timer stats */
#define	REQ_CONFIG		10	/* configure a new peer */
#define	REQ_UNCONFIG		11	/* unconfigure an existing peer */
#define	REQ_SET_SYS_FLAG	12	/* set system flags */
#define	REQ_CLR_SYS_FLAG	13	/* clear system flags */
#define	REQ_MONITOR		14	/* monitor clients */
#define	REQ_NOMONITOR		15	/* stop monitoring clients */
#define	REQ_GET_RESTRICT	16	/* return restrict list */
#define	REQ_RESADDFLAGS		17	/* add flags to restrict list */
#define	REQ_RESSUBFLAGS		18	/* remove flags from restrict list */
#define	REQ_UNRESTRICT		19	/* remove entry from restrict list */
#define	REQ_MON_GETLIST		20	/* return data collected by monitor */
#define	REQ_RESET_STATS		21	/* reset stat counters */
#define	REQ_RESET_PEER		22	/* reset peer stat counters */
#define	REQ_REREAD_KEYS		23	/* reread the encryption key file */
#define	REQ_DO_DIRTY_HACK	24	/* historical interest */
#define	REQ_DONT_DIRTY_HACK	25	/* Ibid. */
#define	REQ_TRUSTKEY		26	/* add a trusted key */
#define	REQ_UNTRUSTKEY		27	/* remove a trusted key */
#define	REQ_AUTHINFO		28	/* return authentication info */
#define REQ_TRAPS		29	/* return currently set traps */
#define	REQ_ADD_TRAP		30	/* add a trap */
#define	REQ_CLR_TRAP		31	/* clear a trap */
#define	REQ_REQUEST_KEY		32	/* define a new request keyid */
#define	REQ_CONTROL_KEY		33	/* define a new control keyid */
#define	REQ_GET_CTLSTATS	34	/* get stats from the control module */
#define	REQ_GET_LEAPINFO	35	/* get leap information */
#define	REQ_GET_CLOCKINFO	36	/* get clock information */
#define	REQ_SET_CLKFUDGE	37	/* set clock fudge factors */
#define	REQ_SET_MAXSKEW		38	/* set the maximum skew factor */
#define	REQ_GET_CLKBUGINFO	39	/* get clock debugging info */
#define	REQ_SET_SELECT_CODE	40	/* set selection algorithm */
#define	REQ_SET_PRECISION	41	/* set clock precision */


/*
 * Flags in the information returns
 */
#define	INFO_FLAG_CONFIG	0x1
#define	INFO_FLAG_SYSPEER	0x2
#define	INFO_FLAG_MINPOLL	0x4
#define	INFO_FLAG_REFCLOCK	0x8
#define	INFO_FLAG_BCLIENT	0x10
#define	INFO_FLAG_PREFER	0x10 /* SHARES BCLIENT bit - ok since mutually exclusive - Oh why ist flags a u_char ? */
#define	INFO_FLAG_AUTHENABLE	0x20
#define	INFO_FLAG_SEL_CANDIDATE	0x40
#define	INFO_FLAG_SHORTLIST	0x80

/*
 * Peer list structure.  Used to return raw lists of peers.  It goes
 * without saying that everything returned is in network byte order.
 */
struct info_peer_list {
	U_LONG address;		/* address of peer */
	u_short port;		/* port number of peer */
	u_char hmode;		/* mode for this peer */
	u_char flags;		/* flags (from above) */
};


/*
 * Peer summary structure.  Sort of the info that ntpdc returns by default.
 */
struct info_peer_summary {
	U_LONG dstadr;		/* local address (zero for undetermined) */
	U_LONG srcadr;		/* source address */
	u_short srcport;	/* source port */
	u_char stratum;		/* stratum of peer */
	s_char hpoll;		/* host polling interval */
	s_char ppoll;		/* peer polling interval */
	u_char reach;		/* reachability register */
	u_char flags;		/* flags, from above */
	u_char hmode;		/* peer mode */
	s_fp delay;		/* peer.estdelay */
	l_fp offset;		/* peer.estoffset */
	u_fp dispersion;	/* peer.estdisp */
};


/*
 * Peer information structure.
 */
struct info_peer {
	U_LONG dstadr;		/* local address */
	U_LONG srcadr;		/* remote address */
	u_short srcport;	/* remote port */
	u_char flags;		/* peer flags */
	u_char leap;		/* peer.leap */
	u_char hmode;		/* peer.hmode */
	u_char pmode;		/* peer.pmode */
	u_char stratum;		/* peer.stratum */
	u_char ppoll;		/* peer.ppoll */
	u_char hpoll;		/* peer.hpoll */
	s_char precision;	/* peer.precision */
	u_char version;		/* peer.version */
	u_char valid;		/* peer.valid */
	u_char reach;		/* peer.reach */
	u_char unreach;		/* peer.unreach */
	u_char trust;		/* peer.trust */
	u_char unused1;
	u_char unused2;
	u_char unused3;
	u_short associd;	/* association ID */
	U_LONG keyid;		/* auth key in use */
	U_LONG pkeyid;		/* peer.pkeyid */
	U_LONG refid;		/* peer.refid */
	U_LONG timer;		/* peer.timer */
	s_fp rootdelay;		/* peer.distance */
	u_fp rootdispersion;	/* peer.dispersion */
	l_fp reftime;		/* peer.reftime */
	l_fp org;		/* peer.org */
	l_fp rec;		/* peer.rec */
	l_fp xmt;		/* peer.xmt */
	s_fp filtdelay[NTP_SHIFT];	/* delay shift register */
	l_fp filtoffset[NTP_SHIFT];	/* offset shift register */
	u_char order[NTP_SHIFT];	/* order of peers from last filter */
	s_fp delay;		/* peer.estdelay */
	u_fp dispersion;	/* peer.estdisp */
	l_fp offset;		/* peer.estoffset */
	U_LONG bdelay[NTP_SHIFT];	/* broadcast delay filters */
	U_LONG estbdelay;	/* broadcast delay */
};


/*
 * Peer statistics structure
 */
struct info_peer_stats {
	U_LONG dstadr;		/* local address */
	U_LONG srcadr;		/* remote address */
	u_short srcport;	/* remote port */
	u_short flags;		/* peer flags */
	U_LONG timereset;	/* time counters were reset */
	U_LONG timereceived;	/* time since a packet received */
	U_LONG timetosend;	/* time until a packet sent */
	U_LONG timereachable;	/* time peer has been reachable */
	U_LONG sent;		/* number sent */
	U_LONG received;	/* number received */
	U_LONG processed;	/* number processed */
	U_LONG badlength;	/* rejected due to bad length */
	U_LONG badauth;		/* rejected due to bad auth */
	U_LONG bogusorg;	/* funny org time stamps */
	U_LONG oldpkt;		/* duplicate packets */
	U_LONG baddelay;	/* dropped due to bad delays */
	U_LONG seldelay;	/* not selected due to delay */
	U_LONG seldisp;		/* not selected due to dispersion */
	U_LONG selbroken;	/* not selected because of brokenness */
	U_LONG selold;		/* not selected because too old */
	u_char candidate;	/* order after falseticker candidate select */
	u_char falseticker;	/* order after resort for falseticker */
	u_char select;		/* order after select */
	u_char select_total;	/* number who made it to selection */
};


/*
 * Loop filter variables
 */
struct info_loop {
	l_fp last_offset;
	l_fp drift_comp;
	U_LONG compliance;
	U_LONG watchdog_timer;
};


/*
 * System info.  Mostly the sys.* variables, plus a few unique to
 * the implementation.
 */
struct info_sys {
	U_LONG peer;		/* system peer address */
	u_char peer_mode;	/* mode we are syncing to peer in */
	u_char leap;		/* system leap bits */
	u_char stratum;		/* our stratum */
	s_char precision;	/* local clock precision */
	s_fp rootdelay;		/* distance from sync source */
	u_fp rootdispersion;	/* dispersion from sync source */
	U_LONG refid;		/* reference ID of sync source */
	l_fp reftime;		/* system reference time */
	U_LONG poll;		/* system poll interval */
	u_short flags;		/* system flags */
	u_char selection;	/* selection algorithm code */
	u_char unused;
	l_fp bdelay;		/* default broadcast delay, a ts fraction */
	l_fp authdelay;		/* default authentication delay */
	u_fp maxskew;		/* maximum skew parameter (obsolete) */
};


/*
 * System stats.  These are collected in the protocol module
 */
struct info_sys_stats {
	U_LONG timeup;		/* time we have been up and running */
	U_LONG timereset;	/* time since these were last cleared */
	U_LONG badstratum;	/* packets claiming an invalid stratum */
	U_LONG oldversionpkt;	/* old version packets received */
	U_LONG newversionpkt;	/* new version packets received */
	U_LONG unknownversion;	/* don't know version packets */
	U_LONG badlength;	/* packets with bad length */
	U_LONG processed;	/* packets processed */
	U_LONG badauth;		/* packets dropped because of authorization */
	U_LONG wanderhold;
};


/*
 * Peer memory statistics.  Collected in the peer module.
 */
struct info_mem_stats {
	U_LONG timereset;	/* time since reset */
	u_short totalpeermem;
	u_short freepeermem;
	U_LONG findpeer_calls;
	U_LONG allocations;
	U_LONG demobilizations;
	u_char hashcount[HASH_SIZE];
};


/*
 * I/O statistics.  Collected in the I/O module
 */
struct info_io_stats {
	U_LONG timereset;	/* time since reset */
	u_short totalrecvbufs;	/* total receive bufs */
	u_short freerecvbufs;	/* free buffers */
	u_short fullrecvbufs;	/* full buffers */
	u_short lowwater;	/* number of times we've added buffers */
	U_LONG dropped;		/* dropped packets */
	U_LONG ignored;		/* ignored packets */
	U_LONG received;	/* received packets */
	U_LONG sent;		/* packets sent */
	U_LONG notsent;		/* packets not sent */
	U_LONG interrupts;	/* interrupts we've handled */
	U_LONG int_received;	/* received by interrupt handler */
};


/*
 * Timer stats.  Guess where from.
 */
struct info_timer_stats {
	U_LONG timereset;	/* time since reset */
	U_LONG alarms;		/* alarms we've handled */
	U_LONG overflows;	/* timer overflows */
	U_LONG xmtcalls;	/* calls to xmit */
};


/*
 * Structure for passing peer configuration information
 */
struct conf_peer {
	U_LONG peeraddr;	/* address to poll */
	u_char hmode;		/* mode, either broadcast, active or client */
	u_char version;		/* version number to poll with */
	u_char minpoll;		/* min host poll interval */
	u_char maxpoll;		/* max host poll interval */
	u_char flags;		/* flags for this request */
	u_char unused;
	U_LONG keyid;		/* key to use for this association */
};

#define	CONF_FLAG_AUTHENABLE	0x1
#define	CONF_FLAG_MINPOLL	0x2
#define CONF_FLAG_PREFER	0x4

/*
 * Structure for passing peer deletion information.  Currently
 * we only pass the address and delete all configured peers with
 * this addess.
 */
struct conf_unpeer {
	U_LONG peeraddr;	/* address of peer */
};


/*
 * Structure for carrying system flags.
 */
struct conf_sys_flags {
	U_LONG flags;
};

/*
 * System flags we can set/clear
 */
#define	SYS_FLAG_BCLIENT	0x1
#define	SYS_FLAG_AUTHENTICATE	0x2

/*
 * Structure used for returning restrict entries
 */
struct info_restrict {
	U_LONG addr;			/* match address */
	U_LONG mask;			/* match mask */
	U_LONG count;			/* number of packets matched */
	u_short flags;			/* restrict flags */
	u_short mflags;			/* match flags */
};


/*
 * Structure used for specifying restrict entries
 */
struct conf_restrict {
	U_LONG addr;			/* match address */
	U_LONG mask;			/* match mask */
	u_short flags;			/* restrict flags */
	u_short mflags;			/* match flags */
};


/*
 * Structure used for returning monitor data
 */
struct info_monitor {	
	U_LONG lasttime;		/* last packet from this host */
	U_LONG firsttime;		/* first time we received a packet */
	U_LONG count;			/* count of packets received */
	U_LONG addr;			/* host address */
	u_short port;			/* port number of last reception */
	u_char mode;			/* mode of last packet */
	u_char version;			/* version number of last packet */
};


/*
 * Structure used for passing indication of flags to clear
 */
struct reset_flags {
	U_LONG flags;
};

#define	RESET_FLAG_ALLPEERS	0x01
#define	RESET_FLAG_IO		0x02
#define	RESET_FLAG_SYS		0x04
#define	RESET_FLAG_MEM		0x08
#define	RESET_FLAG_TIMER	0x10
#define	RESET_FLAG_AUTH		0x20
#define	RESET_FLAG_CTL		0x40

#define	RESET_ALLFLAGS \
	(RESET_FLAG_ALLPEERS|RESET_FLAG_IO|RESET_FLAG_SYS \
	|RESET_FLAG_MEM|RESET_FLAG_TIMER|RESET_FLAG_AUTH|RESET_FLAG_CTL)

/*
 * Structure used to return information concerning the authentication
 * module.
 */
struct info_auth {
	U_LONG timereset;	/* time counters were reset */
	U_LONG numkeys;		/* number of keys we know */
	U_LONG numfreekeys;	/* number of free keys */
	U_LONG keylookups;	/* calls to authhavekey() */
	U_LONG keynotfound;	/* requested key unknown */
	U_LONG encryptions;	/* number of encryptions */
	U_LONG decryptions;	/* number of decryptions */
	U_LONG decryptok;	/* number of successful decryptions */
	U_LONG keyuncached;	/* calls to encrypt/decrypt with uncached key */
};


/*
 * Structure used to pass trap information to the client
 */
struct info_trap {
	U_LONG local_address;	/* local interface address */
	U_LONG trap_address;	/* remote client's address */
	u_short trap_port;	/* remote port number */
	u_short sequence;	/* sequence number */
	U_LONG settime;		/* time trap last set */
	U_LONG origtime;	/* time trap originally set */
	U_LONG resets;		/* number of resets on this trap */
	U_LONG flags;		/* trap flags, as defined in ntp_control.h */
};

/*
 * Structure used to pass add/clear trap information to the client
 */
struct conf_trap {
	U_LONG local_address;	/* local interface address */
	U_LONG trap_address;	/* remote client's address */
	u_short trap_port;	/* remote client's port */
	u_short unused;
};


/*
 * Structure used to return statistics from the control module
 */
struct info_control {
	U_LONG ctltimereset;
	U_LONG numctlreq;		/* number of requests we've received */
	U_LONG numctlbadpkts;		/* number of bad control packets */
	U_LONG numctlresponses;		/* # resp packets sent */
	U_LONG numctlfrags;		/* # of fragments sent */
	U_LONG numctlerrors;		/* number of error responses sent */
	U_LONG numctltooshort;		/* number of too short input packets */
	U_LONG numctlinputresp;		/* number of responses on input */
	U_LONG numctlinputfrag;		/* number of fragments on input */
	U_LONG numctlinputerr;		/* # input pkts with err bit set */
	U_LONG numctlbadoffset;		/* # input pkts with nonzero offset */
	U_LONG numctlbadversion;	/* # input pkts with unknown version */
	U_LONG numctldatatooshort;	/* data too short for count */
	U_LONG numctlbadop;		/* bad op code found in packet */
	U_LONG numasyncmsgs;		/* # async messages we've sent */
};


/*
 * Structure used to return leap information.
 */
struct info_leap {
	u_char sys_leap;		/* current sys_leap */
	u_char leap_indicator;		/* current leap indicator */
	u_char leap_warning;		/* current leap warning */
	u_char leap_bits;		/* leap flags */
	U_LONG leap_timer;		/* seconds to next interrupt */
	U_LONG leap_processcalls;	/* calls to the leap process */
	U_LONG leap_notclose;		/* found leap was not close */
	U_LONG leap_monthofleap;	/* in month of leap */
	U_LONG leap_dayofleap;		/* in day of leap */
	U_LONG leap_hoursfromleap;	/* leap within two hours */
	U_LONG leap_happened;		/* leap second happened */
};

#define	INFO_LEAP_MASK	        0x3	/* flag for leap_bits */
#define	INFO_LEAP_SEENSTRATUM1	0x4	/* server has seen stratum 1 */
#define	INFO_LEAP_OVERRIDE	0x8	/* server will override the leap information */

/*
 * Structure used to return clock information
 */
struct info_clock {
	U_LONG clockadr;
	u_char type;
	u_char flags;
	u_char lastevent;
	u_char currentstatus;
	U_LONG polls;
	U_LONG noresponse;
	U_LONG badformat;
	U_LONG baddata;
	U_LONG timestarted;
	l_fp fudgetime1;
	l_fp fudgetime2;
	LONG fudgeval1;
	LONG fudgeval2;
};


/*
 * Structure used for setting clock fudge factors
 */
struct conf_fudge {
	U_LONG clockadr;
	U_LONG which;
	l_fp fudgetime;
	LONG fudgeval_flags;
};

#define	FUDGE_TIME1	1
#define	FUDGE_TIME2	2
#define	FUDGE_VAL1	3
#define	FUDGE_VAL2	4
#define	FUDGE_FLAGS	5


/*
 * Structure used for returning clock debugging info
 */
#define	NUMCBUGVALUES	16
#define	NUMCBUGTIMES	32

struct info_clkbug {
	U_LONG clockadr;
	u_char nvalues;
	u_char ntimes;
	u_short svalues;
	U_LONG stimes;
	U_LONG values[NUMCBUGVALUES];
	l_fp times[NUMCBUGTIMES];
};
