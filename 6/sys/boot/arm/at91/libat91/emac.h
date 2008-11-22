/******************************************************************************
 *
 * Filename: emac.h
 *
 * Definition of routine to set the MAC address.
 *
 * Revision information:
 *
 * 28AUG2004	kb_admin	initial creation
 *
 * BEGIN_KBDD_BLOCK
 * No warranty, expressed or implied, is included with this software.  It is
 * provided "AS IS" and no warranty of any kind including statutory or aspects
 * relating to merchantability or fitness for any purpose is provided.  All
 * intellectual property rights of others is maintained with the respective
 * owners.  This software is not copyrighted and is intended for reference
 * only.
 * END_BLOCK
 *
 * $FreeBSD$
 *****************************************************************************/


#ifndef _EMAC_H_
#define _EMAC_H_

extern void EMAC_SetMACAddress(unsigned char addr[6]);
extern void SetServerIPAddress(unsigned address);
extern void SetLocalIPAddress(unsigned address);
extern void EMAC_Init(void);
extern void TFTP_Download(unsigned address, char *filename);

#define MAX_RX_PACKETS		8
#define RX_PACKET_SIZE		1536
#define	RX_BUFFER_START		0x21000000
#define	RX_DATA_START		(RX_BUFFER_START + (8 * MAX_RX_PACKETS))

#define ARP_REQUEST		0x0001
#define ARP_REPLY		0x0002
#define PROTOCOL_ARP		0x0806
#define PROTOCOL_IP		0x0800
#define PROTOCOL_UDP		0x11

#define SWAP16(x)	((((x) & 0xff) << 8) | ((x) >> 8))

typedef struct {
	unsigned	address;
	unsigned	size;
} receive_descriptor_t;

typedef struct {

	unsigned char	dest_mac[6];

	unsigned char	src_mac[6];

	unsigned short	frame_type;
	unsigned short	hard_type;
	unsigned short	prot_type;
	unsigned char	hard_size;
	unsigned char	prot_size;

	unsigned short	operation;

	unsigned char	sender_mac[6];
	unsigned char	sender_ip[4];

	unsigned char	target_mac[6];
	unsigned char	target_ip[4];

} __attribute__((__packed__)) arp_header_t;

typedef struct {
	unsigned char	ip_v_hl;
	unsigned char	ip_tos;
	unsigned short	ip_len;
	unsigned short	ip_id;
	unsigned short	ip_off;
	unsigned char	ip_ttl;
	unsigned char	ip_p;
	unsigned short	ip_sum;
	unsigned char	ip_src[4];
	unsigned char	ip_dst[4];
} __attribute__((__packed__)) ip_header_t;

typedef struct {
	unsigned char	dest_mac[6];
	unsigned char	src_mac[6];
	unsigned short	proto_mac;
	unsigned short	packet_length;
	ip_header_t	iphdr;
} __attribute__((__packed__)) transmit_header_t;

typedef struct {
	unsigned short	src_port;
	unsigned short	dst_port;
	unsigned short	udp_len;
	unsigned short	udp_cksum;
} __attribute__((__packed__)) udp_header_t;

typedef struct {
	unsigned short	opcode;
	unsigned short	block_num;
	unsigned char	data[512];
} __attribute__((__packed__)) tftp_header_t;

// Preswap bytes
#define	TFTP_RRQ_OPCODE		0x0100
#define TFTP_WRQ_OPCODE		0x0200
#define TFTP_DATA_OPCODE	0x0300
#define TFTP_ACK_OPCODE		0x0400
#define TFTP_ERROR_OPCODE	0x0500

/* MII registers definition */
#define MII_STS_REG	0x01
#define MII_STS_LINK_STAT	0x04
#ifdef BOOT_KB9202
#define MII_STS2_REG	0x11
#define MII_STS2_LINK	0x400
#define MII_STS2_100TX	0x4000
#define MII_STS2_FDX	0x200
#else
#define MII_SPEC_STS_REG 0x11
#define MII_SSTS_100FDX	0x8000
#define MII_SSTS_100HDX	0x4000
#define MII_SSTS_10FDX	0x2000
#define MII_SSTS_10HDX	0x1000
#endif

extern unsigned char localMACAddr[6];
extern unsigned localMAClow, localMAChigh;
extern unsigned localMACSet, serverMACSet;
extern receive_descriptor_t *p_rxBD;
extern unsigned	lastSize;
extern unsigned localIPSet, serverIPSet;
extern unsigned short	serverPort, localPort;

#endif /* _EMAC_H_ */
