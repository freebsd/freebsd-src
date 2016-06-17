#ifndef IPH5526_IP_H
#define IPH5526_IP_H

#define LLC_SNAP_LEN		0x8

/* Offsets into the ARP frame */
#define ARP_OPCODE_0	(0x6 + LLC_SNAP_LEN)
#define ARP_OPCODE_1	(0x7 + LLC_SNAP_LEN)

int iph5526_probe(struct net_device *dev);
static int fcdev_init(struct net_device *dev);
static int iph5526_open(struct net_device *dev);
static int iph5526_close(struct net_device *dev);
static int iph5526_send_packet(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats * iph5526_get_stats(struct net_device *dev);
static int iph5526_change_mtu(struct net_device *dev, int mtu);


static void rx_net_packet(struct fc_info *fi, u_char *buff_addr, int payload_size);
static void rx_net_mfs_packet(struct fc_info *fi, struct sk_buff *skb);
static int tx_ip_packet(struct sk_buff *skb, unsigned long len, struct fc_info *fi);
static int tx_arp_packet(char *data, unsigned long len, struct fc_info *fi);
#endif

