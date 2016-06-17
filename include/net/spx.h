#ifndef __NET_SPX_H
#define __NET_SPX_H

#include <net/ipx.h>

struct spxhdr
{	__u8	cctl;	
	__u8	dtype;
#define SPX_DTYPE_ECONN	0xFE	/* Finished */
#define SPX_DTYPE_ECACK	0xFF	/* Ok */
	__u16	sconn;	/* Connection ID */
	__u16	dconn;	/* Connection ID */
	__u16	sequence;
	__u16	ackseq;
	__u16	allocseq;
};

struct ipxspxhdr
{	struct ipxhdr	ipx;
	struct spxhdr	spx;
};

#define	SPX_SYS_PKT_LEN	(sizeof(struct ipxspxhdr))

#ifdef __KERNEL__
struct spx_opt
{	int	state;
	int	sndbuf;
	int	retries;	/* Number of WD retries */
	int	retransmits;	/* Number of retransmits */
	int	max_retries;
	int	wd_interval;
	void	*owner;
	__u16	dest_connid;	/* Net order */
	__u16	source_connid;	/* Net order */
	__u16	sequence;	/* Host order - our current pkt # */
	__u16	alloc;		/* Host order - max seq we can rcv now */
	__u16	rmt_ack;	/* Host order - last pkt ACKd by remote */
	__u16	rmt_seq;
	__u16	acknowledge;
	__u16	rmt_alloc;	/* Host order - max seq remote can handle now */
	ipx_address	dest_addr;
	ipx_address	source_addr;
	struct timer_list	watchdog;	/* Idle watch */
	struct timer_list	retransmit;	/* Retransmit timer */
	struct sk_buff_head     rcv_queue;
	struct sk_buff_head	transmit_queue;
	struct sk_buff_head     retransmit_queue;
};

/* Packet connectino control defines */
#define CCTL_SPXII_XHD  0x01    /* SPX2 extended header */
#define CCTL_SPX_UNKNOWN 0x02   /* Unknown (unused ??) */
#define CCTL_SPXII_NEG  0x04    /* Negotiate size */
#define CCTL_SPXII      0x08    /* Set for SPX2 */
#define CCTL_EOM        0x10    /* End of message marker */
#define CCTL_URG        0x20    /* Urgent marker in SPP (not used in SPX?) */
#define CCTL_ACK        0x40    /* Send me an ACK */
#define CCTL_CTL        0x80    /* Control message */
#define CCTL_SYS        CCTL_CTL        /* Spec uses CCTL_SYS */

/* Connection state defines */
#define SPX_CLOSED	7
#define	SPX_CONNECTING	8
#define SPX_CONNECTED	9

/* Packet transmit types - Internal */
#define DATA	0	/* Data */
#define ACK	1	/* Data ACK */
#define WDACK	2	/* WD ACK */
#define CONACK	3	/* Connection Request ACK */
#define	CONREQ	4	/* Connection Request */
#define WDREQ	5	/* WD Request */
#define	DISCON	6	/* Informed Disconnect */
#define	DISACK	7	/* Informed Disconnect ACK */
#define RETRAN	8	/* Int. Retransmit of packet */
#define TQUEUE	9	/* Int. Transmit of a queued packet */

/*
 * These are good canidates for IOcontrol calls
 */

/* Watchdog defines */
#define VERIFY_TIMEOUT  3 * HZ
#define ABORT_TIMEOUT   30 * HZ

/* Packet retransmit defines */
#define RETRY_COUNT     10
#define RETRY_TIME      1 * HZ
#define MAX_RETRY_DELAY 5 * HZ

#endif /* __KERNEL__ */
#endif /* def __NET_SPX_H */
