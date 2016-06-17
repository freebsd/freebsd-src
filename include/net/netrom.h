/*
 *	Declarations of NET/ROM type objects.
 *
 *	Jonathan Naylor G4KLX	9/4/95
 */

#ifndef _NETROM_H
#define _NETROM_H 
#include <linux/netrom.h>

#define	NR_NETWORK_LEN			15
#define	NR_TRANSPORT_LEN		5

#define	NR_PROTO_IP			0x0C

#define	NR_PROTOEXT			0x00
#define	NR_CONNREQ			0x01
#define	NR_CONNACK			0x02
#define	NR_DISCREQ			0x03
#define	NR_DISCACK			0x04
#define	NR_INFO				0x05
#define	NR_INFOACK			0x06

#define	NR_CHOKE_FLAG			0x80
#define	NR_NAK_FLAG			0x40
#define	NR_MORE_FLAG			0x20

/* Define Link State constants. */
enum {
	NR_STATE_0,
	NR_STATE_1,
	NR_STATE_2,
	NR_STATE_3
};

#define	NR_COND_ACK_PENDING		0x01
#define	NR_COND_REJECT			0x02
#define	NR_COND_PEER_RX_BUSY		0x04
#define	NR_COND_OWN_RX_BUSY		0x08

#define NR_DEFAULT_T1			(120 * HZ)	/* Outstanding frames - 120 seconds */
#define NR_DEFAULT_T2			(5   * HZ)	/* Response delay     - 5 seconds */
#define NR_DEFAULT_N2			3		/* Number of Retries - 3 */
#define	NR_DEFAULT_T4			(180 * HZ)	/* Busy Delay - 180 seconds */
#define	NR_DEFAULT_IDLE			(0 * 60 * HZ)	/* No Activity Timeout - none */
#define	NR_DEFAULT_WINDOW		4		/* Default Window Size - 4 */
#define	NR_DEFAULT_OBS			6		/* Default Obsolescence Count - 6 */
#define	NR_DEFAULT_QUAL			10		/* Default Neighbour Quality - 10 */
#define	NR_DEFAULT_TTL			16		/* Default Time To Live - 16 */
#define	NR_DEFAULT_ROUTING		1		/* Is routing enabled ? */
#define	NR_DEFAULT_FAILS		2		/* Link fails until route fails */

#define NR_MODULUS 			256
#define NR_MAX_WINDOW_SIZE		127			/* Maximum Window Allowable - 127 */
#define	NR_MAX_PACKET_SIZE		236			/* Maximum Packet Length - 236 */

typedef struct {
	ax25_address		user_addr, source_addr, dest_addr;
	struct net_device		*device;
	unsigned char		my_index,   my_id;
	unsigned char		your_index, your_id;
	unsigned char		state, condition, bpqext, window;
	unsigned short		vs, vr, va, vl;
	unsigned char		n2, n2count;
	unsigned long		t1, t2, t4, idle;
	unsigned short		fraglen;
	struct timer_list	t1timer;
	struct timer_list	t2timer;
	struct timer_list	t4timer;
	struct timer_list	idletimer;
	struct sk_buff_head	ack_queue;
	struct sk_buff_head	reseq_queue;
	struct sk_buff_head	frag_queue;
	struct sock		*sk;		/* Backlink to socket */
} nr_cb;

struct nr_neigh {
	struct nr_neigh *next;
	ax25_address    callsign;
	ax25_digi       *digipeat;
	ax25_cb		*ax25;
	struct net_device   *dev;
	unsigned char   quality;
	unsigned char   locked;
	unsigned short  count;
	unsigned int    number;
	unsigned char	failed;
};

struct nr_route {
	unsigned char   quality;
	unsigned char   obs_count;
	struct nr_neigh *neighbour;
};

struct nr_node {
	struct nr_node  *next;
	ax25_address    callsign;
	char		mnemonic[7];
	unsigned char   which;
	unsigned char   count;
	struct nr_route routes[3];
};

/* af_netrom.c */
extern int  sysctl_netrom_default_path_quality;
extern int  sysctl_netrom_obsolescence_count_initialiser;
extern int  sysctl_netrom_network_ttl_initialiser;
extern int  sysctl_netrom_transport_timeout;
extern int  sysctl_netrom_transport_maximum_tries;
extern int  sysctl_netrom_transport_acknowledge_delay;
extern int  sysctl_netrom_transport_busy_delay;
extern int  sysctl_netrom_transport_requested_window_size;
extern int  sysctl_netrom_transport_no_activity_timeout;
extern int  sysctl_netrom_routing_control;
extern int  sysctl_netrom_link_fails_count;
extern int  nr_rx_frame(struct sk_buff *, struct net_device *);
extern void nr_destroy_socket(struct sock *);

/* nr_dev.c */
extern int  nr_rx_ip(struct sk_buff *, struct net_device *);
extern int  nr_init(struct net_device *);

/* nr_in.c */
extern int  nr_process_rx_frame(struct sock *, struct sk_buff *);

/* nr_loopback.c */
extern void nr_loopback_init(void);
extern void nr_loopback_clear(void);
extern int  nr_loopback_queue(struct sk_buff *);

/* nr_out.c */
extern void nr_output(struct sock *, struct sk_buff *);
extern void nr_send_nak_frame(struct sock *);
extern void nr_kick(struct sock *);
extern void nr_transmit_buffer(struct sock *, struct sk_buff *);
extern void nr_establish_data_link(struct sock *);
extern void nr_enquiry_response(struct sock *);
extern void nr_check_iframes_acked(struct sock *, unsigned short);

/* nr_route.c */
extern void nr_rt_device_down(struct net_device *);
extern struct net_device *nr_dev_first(void);
extern struct net_device *nr_dev_get(ax25_address *);
extern int  nr_rt_ioctl(unsigned int, void *);
extern void nr_link_failed(ax25_cb *, int);
extern int  nr_route_frame(struct sk_buff *, ax25_cb *);
extern int  nr_nodes_get_info(char *, char **, off_t, int);
extern int  nr_neigh_get_info(char *, char **, off_t, int);
extern void nr_rt_free(void);

/* nr_subr.c */
extern void nr_clear_queues(struct sock *);
extern void nr_frames_acked(struct sock *, unsigned short);
extern void nr_requeue_frames(struct sock *);
extern int  nr_validate_nr(struct sock *, unsigned short);
extern int  nr_in_rx_window(struct sock *, unsigned short);
extern void nr_write_internal(struct sock *, int);
extern void nr_transmit_refusal(struct sk_buff *, int);
extern void nr_disconnect(struct sock *, int);

/* nr_timer.c */
extern void nr_start_heartbeat(struct sock *);
extern void nr_start_t1timer(struct sock *);
extern void nr_start_t2timer(struct sock *);
extern void nr_start_t4timer(struct sock *);
extern void nr_start_idletimer(struct sock *);
extern void nr_stop_heartbeat(struct sock *);
extern void nr_stop_t1timer(struct sock *);
extern void nr_stop_t2timer(struct sock *);
extern void nr_stop_t4timer(struct sock *);
extern void nr_stop_idletimer(struct sock *);
extern int  nr_t1timer_running(struct sock *);

/* sysctl_net_netrom.c */
extern void nr_register_sysctl(void);
extern void nr_unregister_sysctl(void);

#endif
