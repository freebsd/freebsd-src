/*
 * Rx protocol format
 *
 * $Id: rx.h,v 1.3 2000/10/03 02:55:02 itojun Exp $
 */

#define FS_RX_PORT	7000
#define CB_RX_PORT	7001
#define PROT_RX_PORT	7002
#define VLDB_RX_PORT	7003
#define KAUTH_RX_PORT	7004
#define VOL_RX_PORT	7005
#define ERROR_RX_PORT	7006		/* Doesn't seem to be used */
#define BOS_RX_PORT	7007

#ifndef AFSNAMEMAX
#define AFSNAMEMAX 256
#endif

#ifndef AFSOPAQUEMAX
#define AFSOPAQUEMAX 1024
#endif

#define PRNAMEMAX 64
#define VLNAMEMAX 65
#define KANAMEMAX 64
#define BOSNAMEMAX 256

#define	PRSFS_READ		1 /* Read files */
#define	PRSFS_WRITE		2 /* Write files */
#define	PRSFS_INSERT		4 /* Insert files into a directory */
#define	PRSFS_LOOKUP		8 /* Lookup files into a directory */
#define	PRSFS_DELETE		16 /* Delete files */
#define	PRSFS_LOCK		32 /* Lock files */
#define	PRSFS_ADMINISTER	64 /* Change ACL's */

struct rx_header {
	u_int32_t epoch;
	u_int32_t cid;
	u_int32_t callNumber;
	u_int32_t seq;
	u_int32_t serial;
	u_int8_t type;
#define RX_PACKET_TYPE_DATA		1
#define RX_PACKET_TYPE_ACK		2
#define RX_PACKET_TYPE_BUSY		3
#define RX_PACKET_TYPE_ABORT		4
#define RX_PACKET_TYPE_ACKALL		5
#define RX_PACKET_TYPE_CHALLENGE	6
#define RX_PACKET_TYPE_RESPONSE		7
#define RX_PACKET_TYPE_DEBUG		8
#define RX_PACKET_TYPE_PARAMS		9
#define RX_PACKET_TYPE_VERSION		13
	u_int8_t flags;
#define RX_CLIENT_INITIATED	1
#define RX_REQUEST_ACK		2
#define RX_LAST_PACKET		4
#define RX_MORE_PACKETS		8
#define RX_FREE_PACKET		16
	u_int8_t userStatus;
	u_int8_t securityIndex;
	u_int16_t spare;		/* How clever: even though the AFS */
	u_int16_t serviceId;		/* header files indicate that the */
};					/* serviceId is first, it's really */
					/* encoded _after_ the spare field */
					/* I wasted a day figuring that out! */

#define NUM_RX_FLAGS 5

#define RX_MAXACKS 255

struct rx_ackPacket {
	u_int16_t bufferSpace;		/* Number of packet buffers available */
	u_int16_t maxSkew;		/* Max diff between ack'd packet and */
					/* highest packet received */
	u_int32_t firstPacket;		/* The first packet in ack list */
	u_int32_t previousPacket;	/* Previous packet recv'd (obsolete) */
	u_int32_t serial;		/* # of packet that prompted the ack */
	u_int8_t reason;		/* Reason for acknowledgement */
	u_int8_t nAcks;			/* Number of acknowledgements */
	u_int8_t acks[RX_MAXACKS];	/* Up to RX_MAXACKS acknowledgements */
};

/*
 * Values for the acks array
 */

#define RX_ACK_TYPE_NACK	0	/* Don't have this packet */
#define RX_ACK_TYPE_ACK		1	/* I have this packet */
