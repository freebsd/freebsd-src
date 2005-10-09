#ifndef L2_PACKET_H
#define L2_PACKET_H

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#ifndef ETH_P_EAPOL
#define ETH_P_EAPOL 0x888e
#endif

#ifndef ETH_P_RSN_PREAUTH
#define ETH_P_RSN_PREAUTH 0x88c7
#endif

struct l2_packet_data;

struct l2_ethhdr {
	u8 h_dest[ETH_ALEN];
	u8 h_source[ETH_ALEN];
	u16 h_proto;
} __attribute__ ((packed));

struct l2_packet_data * l2_packet_init(
	const char *ifname, const u8 *own_addr, unsigned short protocol,
	void (*rx_callback)(void *ctx, unsigned char *src_addr,
			    unsigned char *buf, size_t len),
	void *rx_callback_ctx);
void l2_packet_deinit(struct l2_packet_data *l2);

int l2_packet_get_own_addr(struct l2_packet_data *l2, u8 *addr);
int l2_packet_send(struct l2_packet_data *l2, u8 *buf, size_t len);
void l2_packet_set_rx_l2_hdr(struct l2_packet_data *l2, int rx_l2_hdr);

#endif /* L2_PACKET_H */
