/*	$NetBSD: if_ieee80211.h,v 1.5 2000/07/21 04:47:40 onoe Exp $	*/
/* $FreeBSD$ */

#ifndef _NET_IF_IEEE80211_H_
#define _NET_IF_IEEE80211_H_

/*
 * generic definitions for IEEE 802.11 frames
 */
struct ieee80211_frame {
	u_int8_t	i_fc[2];
	u_int8_t	i_dur[2];
	u_int8_t	i_addr1[ETHER_ADDR_LEN];
	u_int8_t	i_addr2[ETHER_ADDR_LEN];
	u_int8_t	i_addr3[ETHER_ADDR_LEN];
	u_int8_t	i_seq[2];
	/* possibly followed by addr4[ETHER_ADDR_LEN]; */
};

#define	IEEE80211_FC0_VERSION_MASK		0x03
#define	IEEE80211_FC0_VERSION_0			0x00
#define	IEEE80211_FC0_TYPE_MASK			0x0c
#define	IEEE80211_FC0_TYPE_MGT			0x00
#define	IEEE80211_FC0_TYPE_CTL			0x04
#define	IEEE80211_FC0_TYPE_DATA			0x08

#define	IEEE80211_FC0_SUBTYPE_MASK		0xf0
/* for TYPE_MGT */
#define	IEEE80211_FC0_SUBTYPE_ASSOC_REQ		0x00
#define	IEEE80211_FC0_SUBTYPE_ASSOC_RESP	0x10
#define	IEEE80211_FC0_SUBTYPE_REASSOC_REQ	0x20
#define	IEEE80211_FC0_SUBTYPE_REASSOC_RESP	0x30
#define	IEEE80211_FC0_SUBTYPE_PROBE_REQ		0x40
#define	IEEE80211_FC0_SUBTYPE_PROBE_RESP	0x50
#define	IEEE80211_FC0_SUBTYPE_BEACON		0x80
#define	IEEE80211_FC0_SUBTYPE_ATIM		0x90
#define	IEEE80211_FC0_SUBTYPE_DISASSOC		0xa0
#define	IEEE80211_FC0_SUBTYPE_AUTH		0xb0
#define	IEEE80211_FC0_SUBTYPE_DEAUTH		0xc0
/* for TYPE_CTL */
#define	IEEE80211_FC0_SUBTYPE_PS_POLL		0xa0
#define	IEEE80211_FC0_SUBTYPE_RTS		0xb0
#define	IEEE80211_FC0_SUBTYPE_CTS		0xc0
#define	IEEE80211_FC0_SUBTYPE_ACK		0xd0
#define	IEEE80211_FC0_SUBTYPE_CF_END		0xe0
#define	IEEE80211_FC0_SUBTYPE_CF_END_ACK	0xf0
/* for TYPE_DATA (bit combination) */
#define IEEE80211_FC0_SUBTYPE_DATA		0x00
#define	IEEE80211_FC0_SUBTYPE_CF_ACK		0x10
#define	IEEE80211_FC0_SUBTYPE_CF_POLL		0x20
#define	IEEE80211_FC0_SUBTYPE_CF_ACPL		0x30
#define	IEEE80211_FC0_SUBTYPE_NODATA		0x40
#define	IEEE80211_FC0_SUBTYPE_CFACK		0x50
#define	IEEE80211_FC0_SUBTYPE_CFPOLL		0x60
#define	IEEE80211_FC0_SUBTYPE_CF_ACK_CF_ACK	0x70

#define	IEEE80211_FC1_DIR_MASK			0x03
#define	IEEE80211_FC1_DIR_NODS			0x00	/* STA->STA */
#define	IEEE80211_FC1_DIR_TODS			0x01	/* STA->AP  */
#define	IEEE80211_FC1_DIR_FROMDS		0x02	/* AP ->STA */
#define	IEEE80211_FC1_DIR_DSTODS		0x03	/* AP ->AP  */

#define	IEEE80211_FC1_MORE_FRAG			0x04
#define	IEEE80211_FC1_RETRY			0x08
#define	IEEE80211_FC1_PWR_MGT			0x10
#define	IEEE80211_FC1_MORE_DATA			0x20
#define	IEEE80211_FC1_WEP			0x40
#define	IEEE80211_FC1_ORDER			0x80

#define	IEEE80211_NWID_LEN			32

/*
 * BEACON management packets
 *
 *	octect timestamp[8]
 *	octect beacon interval[2]
 *	octect capability information[2]
 *	information element
 *		octect elemid
 *		octect length
 *		octect information[length[
 */
typedef u_int8_t *	ieee80211_mgt_beacon_t;

#define IEEE80211_BEACON_INTERVAL(beacon) \
	(beacon[8] + (beacon[9] << 8))
#define IEEE80211_BEACON_CAPABILITY(beacon) \
	(beacon[10] + (beacon[11] << 8))

#define	IEEE80211_CAPINFO_ESS			0x01
#define	IEEE80211_CAPINFO_IBSS			0x02
#define	IEEE80211_CAPINFO_CF_POLLABLE		0x04
#define	IEEE80211_CAPINFO_CF_POLLREQ		0x08
#define	IEEE80211_CAPINFO_PRIVACY		0x10


/*
 * Management information elements
 */
struct ieee80211_information {
	char	ssid[IEEE80211_NWID_LEN+1];
	struct rates {
		u_int8_t 	*p;
	} rates;
	struct fh {
		u_int16_t 	dwell;
		u_int8_t 	set;
		u_int8_t 	pattern;
		u_int8_t 	index;
	} fh;
	struct ds {
		u_int8_t	channel;
	} ds;
	struct cf {
		u_int8_t	count;
		u_int8_t	period;
		u_int8_t	maxdur[2];
		u_int8_t	dur[2];
	} cf;
	struct tim {
		u_int8_t 	count;
		u_int8_t 	period;
		u_int8_t 	bitctl;
		/* u_int8_t 	pvt[251]; The driver never needs to use this */
	} tim;
	struct ibss {
	    	u_int16_t	atim;
	} ibss;
	struct challenge {
		u_int8_t 	*p;
		u_int8_t	len;
	} challenge;
};

#define	IEEE80211_ELEMID_SSID			0
#define	IEEE80211_ELEMID_RATES			1
#define	IEEE80211_ELEMID_FHPARMS		2
#define	IEEE80211_ELEMID_DSPARMS		3
#define	IEEE80211_ELEMID_CFPARMS		4
#define	IEEE80211_ELEMID_TIM			5
#define	IEEE80211_ELEMID_IBSSPARMS		6
#define	IEEE80211_ELEMID_CHALLENGE		16

/*
 * AUTH management packets
 *
 *	octect algo[2]
 *	octect seq[2]
 *	octect status[2]
 *	octect chal.id
 *	octect chal.length
 *	octect chal.text[253]
 */
typedef u_int8_t *	ieee80211_mgt_auth_t;

#define IEEE80211_AUTH_ALGORITHM(auth) \
    (auth[0] + (auth[1] << 8))
#define IEEE80211_AUTH_TRANSACTION(auth) \
    (auth[2] + (auth[3] << 8))
#define IEEE80211_AUTH_STATUS(auth) \
    (auth[4] + (auth[5] << 8))

#define	IEEE80211_AUTH_ALG_OPEN			0x0000
#define	IEEE80211_AUTH_ALG_SHARED		0x0001

#define IEEE80211_AUTH_OPEN_REQUEST		1
#define IEEE80211_AUTH_OPEN_RESPONSE		2

#define IEEE80211_AUTH_SHARED_REQUEST		1
#define IEEE80211_AUTH_SHARED_CHALLENGE		2
#define IEEE80211_AUTH_SHARED_RESPONSE		3
#define IEEE80211_AUTH_SHARED_PASS		4

/*
 * Reason codes
 *
 * Unlisted codes are reserved
 */
#define	IEEE80211_REASON_UNSPECIFIED		1
#define	IEEE80211_REASON_AUTH_EXPIRE		2
#define	IEEE80211_REASON_AUTH_LEAVE		3
#define	IEEE80211_REASON_ASSOC_EXPIRE		4
#define	IEEE80211_REASON_ASSOC_TOOMANY		5
#define	IEEE80211_REASON_NOT_AUTHED		6  
#define	IEEE80211_REASON_NOT_ASSOCED		7
#define	IEEE80211_REASON_ASSOC_LEAVE		8
#define	IEEE80211_REASON_ASSOC_NOT_AUTHED	9

/*
 * Status code
 *
 * Unlisted codes are reserved
 */
#define IEEE80211_STATUS_SUCCESS		0x0000
#define	IEEE80211_STATUS_UNSPECIFIED		1
#define	IEEE80211_STATUS_CAPINFO		10
#define	IEEE80211_STATUS_NOT_ASSOCED		11
#define	IEEE80211_STATUS_OTHER			12
#define	IEEE80211_STATUS_ALG			13
#define	IEEE80211_STATUS_SEQUENCE		14
#define	IEEE80211_STATUS_CHALLENGE		15
#define	IEEE80211_STATUS_TIMEOUT		16

#define	IEEE80211_WEP_KEYLEN			5	/* 40bit */
#define	IEEE80211_WEP_IVLEN			3	/* 24bit */
#define	IEEE80211_WEP_KIDLEN			1	/* 1 octet */
#define	IEEE80211_WEP_CRCLEN			4	/* CRC-32 */
#define	IEEE80211_WEP_NKID			4	/* number of key ids */

/* nwid is pointed at by ifr.ifr_data */
struct ieee80211_nwid {
	u_int8_t	i_len;
	u_int8_t	i_nwid[IEEE80211_NWID_LEN];
};

#define	SIOCS80211NWID		_IOWR('i', 230, struct ifreq)
#define	SIOCG80211NWID		_IOWR('i', 231, struct ifreq)

/* the first member must be matched with struct ifreq */
struct ieee80211_nwkey {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	int		i_wepon;		/* wep enabled flag */
	int		i_defkid;		/* default encrypt key id */
	struct {
		int		i_keylen;
		u_int8_t	*i_keydat;
	}		i_key[IEEE80211_WEP_NKID];
};
#define	SIOCS80211NWKEY		 _IOW('i', 232, struct ieee80211_nwkey)
#define	SIOCG80211NWKEY		_IOWR('i', 233, struct ieee80211_nwkey)

#define IEEE80211_WEP_NOSUP	-1
#define IEEE80211_WEP_OFF	0
#define IEEE80211_WEP_ON	1
#define IEEE80211_WEP_MIXED	2

#define IEEE80211_AUTH_NONE	0
#define IEEE80211_AUTH_OPEN	1
#define IEEE80211_AUTH_SHARED	2

#define IEEE80211_POWERSAVE_NOSUP	-1
#define IEEE80211_POWERSAVE_OFF		0
#define IEEE80211_POWERSAVE_CAM		1
#define IEEE80211_POWERSAVE_PSP		2
#define IEEE80211_POWERSAVE_PSP_CAM	3
#define IEEE80211_POWERSAVE_ON		IEEE80211_POWERSAVE_CAM

/* the first member must be matched with struct ifreq */
struct ieee80211req {
	char		i_name[IFNAMSIZ];	/* if_name, e.g. "wi0" */
	u_int16_t	i_type;			/* req type */
	int16_t		i_val;			/* Index or simple value */
	int16_t		i_len;			/* Index or simple value */
	void		*i_data;		/* Extra data */
};
#define	SIOCS80211		 _IOW('i', 234, struct ieee80211req)
#define	SIOCG80211		_IOWR('i', 235, struct ieee80211req)

#define IEEE80211_IOC_SSID		1
#define IEEE80211_IOC_NUMSSIDS		2
#define IEEE80211_IOC_WEP		3
#define IEEE80211_IOC_WEPKEY		4
#define IEEE80211_IOC_NUMWEPKEYS	5
#define IEEE80211_IOC_WEPTXKEY		6
#define IEEE80211_IOC_AUTHMODE		7
#define IEEE80211_IOC_STATIONNAME	8
#define IEEE80211_IOC_CHANNEL		9
#define IEEE80211_IOC_POWERSAVE		10
#define IEEE80211_IOC_POWERSAVESLEEP	11

#endif /* !_NET_IF_IEEE80211_H_ */
