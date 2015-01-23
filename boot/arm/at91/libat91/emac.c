/*******************************************************************************
 *
 * Filename: emac.c
 *
 * Instantiation of routines for MAC/ethernet functions supporting tftp.
 *
 * Revision information:
 *
 * 28AUG2004	kb_admin	initial creation
 * 08JAN2005	kb_admin	added tftp download
 *					also adapted from external sources
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
 ******************************************************************************/

#include "at91rm9200.h"
#include "at91rm9200_lowlevel.h"
#include "emac.h"
#include "lib.h"

/* ****************************** GLOBALS *************************************/

/* ********************** PRIVATE FUNCTIONS/DATA ******************************/

static receive_descriptor_t *p_rxBD;
static unsigned short localPort;
static unsigned short serverPort;
static unsigned serverMACSet;
static unsigned localIPSet, serverIPSet;
static unsigned	lastSize;
static unsigned char serverMACAddr[6];
static unsigned char localIPAddr[4], serverIPAddr[4];
static int	ackBlock;
static char *dlAddress;

static unsigned transmitBuffer[1024 / sizeof(unsigned)];
static unsigned tftpSendPacket[256 / sizeof(unsigned)];

/*
 * .KB_C_FN_DEFINITION_START
 * unsigned short IP_checksum(unsigned short *p, int len)
 *  This private function calculates the IP checksum for various headers.
 * .KB_C_FN_DEFINITION_END
 */
static unsigned short
IP_checksum(unsigned short *p, int len) 
{
	unsigned	i, t;

	len &= ~1;

	for (i=0,t=0; i<len; i+=2, ++p)
		t += SWAP16(*p);

	t = (t & 0xffff) + (t >> 16);
	return (~t);									
}


/*
 * .KB_C_FN_DEFINITION_START
 * void GetServerAddress(void)
 *  This private function sends an ARP request to determine the server MAC.
 * .KB_C_FN_DEFINITION_END
 */
static void
GetServerAddress(void)
{
	arp_header_t	*p_ARP;

	p_ARP = (arp_header_t*)transmitBuffer;

	p_memset((char*)p_ARP->dest_mac, 0xFF, 6);

	memcpy(p_ARP->src_mac, localMACAddr, 6);

	p_ARP->frame_type = SWAP16(PROTOCOL_ARP);
	p_ARP->hard_type  = SWAP16(1);
	p_ARP->prot_type  = SWAP16(PROTOCOL_IP);
	p_ARP->hard_size  = 6;
	p_ARP->prot_size  = 4;
	p_ARP->operation  = SWAP16(ARP_REQUEST);

	memcpy(p_ARP->sender_mac, localMACAddr, 6);
	memcpy(p_ARP->sender_ip, localIPAddr, 4);
	p_memset((char*)p_ARP->target_mac, 0, 6);
	memcpy(p_ARP->target_ip, serverIPAddr, 4);

	// wait until transmit is available
	while (!(*AT91C_EMAC_TSR & AT91C_EMAC_BNQ)) 
		continue;

  	*AT91C_EMAC_TSR |= AT91C_EMAC_COMP;
	*AT91C_EMAC_TAR = (unsigned)transmitBuffer;
	*AT91C_EMAC_TCR = 0x40;
}


/*
 * .KB_C_FN_DEFINITION_START
 * void Send_TFTP_Packet(char *tftpData, unsigned tftpLength)
 *  This private function initializes and send a TFTP packet.
 * .KB_C_FN_DEFINITION_END
 */
static void
Send_TFTP_Packet(char *tftpData, unsigned tftpLength)
{
	transmit_header_t	*macHdr = (transmit_header_t*)tftpSendPacket;
	ip_header_t		*ipHdr;
	udp_header_t		*udpHdr;
	unsigned		t_checksum;

	memcpy(macHdr->dest_mac, serverMACAddr, 6);
	memcpy(macHdr->src_mac, localMACAddr, 6);
	macHdr->proto_mac = SWAP16(PROTOCOL_IP);

	ipHdr = (ip_header_t*)&macHdr->packet_length;

	ipHdr->ip_v_hl = 0x45;
	ipHdr->ip_tos = 0;
	ipHdr->ip_len = SWAP16(28 + tftpLength);
	ipHdr->ip_id = 0;
	ipHdr->ip_off = SWAP16(0x4000);
	ipHdr->ip_ttl = 64;
	ipHdr->ip_p = PROTOCOL_UDP;
	ipHdr->ip_sum = 0;

	memcpy(ipHdr->ip_src, localIPAddr, 4);
	memcpy(ipHdr->ip_dst, serverIPAddr, 4);

	ipHdr->ip_sum = SWAP16(IP_checksum((unsigned short*)ipHdr, 20));

	udpHdr = (udp_header_t*)(ipHdr + 1);

	udpHdr->src_port  = localPort;
	udpHdr->dst_port  = serverPort;
	udpHdr->udp_len   = SWAP16(8 + tftpLength);
	udpHdr->udp_cksum = 0;

	memcpy((char *)udpHdr+8, tftpData, tftpLength);

	t_checksum = IP_checksum((unsigned short*)ipHdr + 6, (16 + tftpLength));

	t_checksum = (~t_checksum) & 0xFFFF;
	t_checksum += 25 + tftpLength;

	t_checksum = (t_checksum & 0xffff) + (t_checksum >> 16);
	t_checksum = (~t_checksum) & 0xFFFF;

	udpHdr->udp_cksum = SWAP16(t_checksum);

	while (!(*AT91C_EMAC_TSR & AT91C_EMAC_BNQ))
		continue;

  	*AT91C_EMAC_TSR |= AT91C_EMAC_COMP;
	*AT91C_EMAC_TAR = (unsigned)tftpSendPacket;
	*AT91C_EMAC_TCR = 42 + tftpLength;
}


/*
 * .KB_C_FN_DEFINITION_START
 * void TFTP_RequestFile(char *filename)
 *  This private function sends a RRQ packet to the server.
 * .KB_C_FN_DEFINITION_END
 */
static void
TFTP_RequestFile(char *filename)
{
	tftp_header_t	tftpHeader;
	char		*cPtr, *ePtr, *mPtr;
	unsigned	length;

	tftpHeader.opcode = TFTP_RRQ_OPCODE;

	cPtr = (char*)&(tftpHeader.block_num);

	ePtr = strcpy(cPtr, filename);
	mPtr = strcpy(ePtr, "octet");

	length = mPtr - cPtr;
	length += 2;
	
	Send_TFTP_Packet((char*)&tftpHeader, length);
}


/*
 * .KB_C_FN_DEFINITION_START
 * void TFTP_ACK_Data(char *data, unsigned short block_num, unsigned short len)
 *  This private function sends an ACK packet to the server.
 * .KB_C_FN_DEFINITION_END
 */
static void
TFTP_ACK_Data(unsigned char *data, unsigned short block_num, unsigned short len)
{
	tftp_header_t	tftpHeader;

	if (block_num == (ackBlock + 1)) {
		++ackBlock;
		memcpy(dlAddress, data, len);
		dlAddress += len;
		lastSize += len;
		if (ackBlock % 128 == 0)
			printf("tftp: %u kB\r", lastSize / 1024);
	}
	tftpHeader.opcode = TFTP_ACK_OPCODE;
	tftpHeader.block_num = SWAP16(ackBlock);
	Send_TFTP_Packet((char*)&tftpHeader, 4);
	if (len < 512) {
		ackBlock = -2;
		printf("tftp: %u byte\n", lastSize);
	}
}


/*
 * .KB_C_FN_DEFINITION_START
 * void CheckForNewPacket(ip_header_t *pHeader)
 *  This private function polls for received ethernet packets and handles
 * any here.
 * .KB_C_FN_DEFINITION_END
 */
static int
CheckForNewPacket(ip_header_t *pHeader)
{
	unsigned short	*pFrameType;
	unsigned	i;
	char		*pData;
	ip_header_t	*pIpHeader;
	arp_header_t	*p_ARP;
	int		process = 0;
	
	process = 0;	
	for (i = 0; i < MAX_RX_PACKETS; ++i) {				
		if(p_rxBD[i].address & 0x1) {
			process = 1;	
			(*AT91C_EMAC_RSR) |= (*AT91C_EMAC_RSR);
			break;				
		}
	}
		
	if (!process)
		return (0);
	process = i;
		
	pFrameType = (unsigned short *)((p_rxBD[i].address & 0xFFFFFFFC) + 12);
	pData      = (char *)(p_rxBD[i].address & 0xFFFFFFFC);

	switch (*pFrameType) {

	case SWAP16(PROTOCOL_ARP):
		p_ARP = (arp_header_t*)pData;
		if (p_ARP->operation == SWAP16(ARP_REPLY)) {
			// check if new server info is available
			if ((!serverMACSet) &&
				(!(p_memcmp((char*)p_ARP->sender_ip,
					(char*)serverIPAddr, 4)))) {

				serverMACSet = 1;
				memcpy(serverMACAddr, p_ARP->sender_mac, 6);
			}
		} else if (p_ARP->operation == SWAP16(ARP_REQUEST)) {
			// ARP REPLY operation
			p_ARP->operation =  SWAP16(ARP_REPLY);

			// Fill the dest address and src address
			for (i = 0; i <6; i++) {
				// swap ethernet dest address and ethernet src address
				pData[i] = pData[i+6];
				pData[i+6] = localMACAddr[i];
				// swap sender ethernet address and target ethernet address
				pData[i+22] = localMACAddr[i];
				pData[i+32] = pData[i+6];
			}									

			// swap sender IP address and target IP address
			for (i = 0; i<4; i++) {				
				pData[i+38] = pData[i+28];
				pData[i+28] = localIPAddr[i];
			}

			if (!(*AT91C_EMAC_TSR & AT91C_EMAC_BNQ)) break;

		  	*AT91C_EMAC_TSR |= AT91C_EMAC_COMP;
			*AT91C_EMAC_TAR = (unsigned)pData;
 			*AT91C_EMAC_TCR = 0x40;
		}
		break;
	case SWAP16(PROTOCOL_IP):
		pIpHeader = (ip_header_t*)(pData + 14);			
		memcpy(pHeader, pIpHeader, sizeof(ip_header_t));
		
		if (pIpHeader->ip_p == PROTOCOL_UDP) {
			udp_header_t	*udpHdr;
			tftp_header_t	*tftpHdr;

			udpHdr = (udp_header_t*)((char*)pIpHeader+20);
			tftpHdr = (tftp_header_t*)((char*)udpHdr + 8);

			if (udpHdr->dst_port != localPort)
				break;

			if (tftpHdr->opcode != TFTP_DATA_OPCODE)
				break;

			if (ackBlock == -1) {
				if (tftpHdr->block_num != SWAP16(1))
					break;
				serverPort = udpHdr->src_port;
				ackBlock = 0;
			}

			if (serverPort != udpHdr->src_port)
				break;

			TFTP_ACK_Data(tftpHdr->data,
			    SWAP16(tftpHdr->block_num),
			    SWAP16(udpHdr->udp_len) - 12);
		}
	}
	p_rxBD[process].address &= ~0x01;
	return (1);
}


/*
 * .KB_C_FN_DEFINITION_START
 * unsigned short AT91F_MII_ReadPhy (AT91PS_EMAC pEmac, unsigned char addr)
 *  This private function reads the PHY device.
 * .KB_C_FN_DEFINITION_END
 */
#ifndef BOOT_BWCT
static unsigned short
AT91F_MII_ReadPhy (AT91PS_EMAC pEmac, unsigned char addr)
{
	unsigned value = 0x60020000 | (addr << 18);

	pEmac->EMAC_CTL |= AT91C_EMAC_MPE;
	pEmac->EMAC_MAN = value;
  	while(!((pEmac->EMAC_SR) & AT91C_EMAC_IDLE));
	pEmac->EMAC_CTL &= ~AT91C_EMAC_MPE;
	return (pEmac->EMAC_MAN & 0x0000ffff);
}
#endif

/*
 * .KB_C_FN_DEFINITION_START
 * unsigned short AT91F_MII_WritePhy (AT91PS_EMAC pEmac, unsigned char addr, unsigned short s)
 *  This private function writes the PHY device.
 * .KB_C_FN_DEFINITION_END
 */
#ifdef BOOT_TSC
static unsigned short
AT91F_MII_WritePhy (AT91PS_EMAC pEmac, unsigned char addr, unsigned short s)
{
	unsigned value = 0x50020000 | (addr << 18) | s;

	pEmac->EMAC_CTL |= AT91C_EMAC_MPE;
	pEmac->EMAC_MAN = value;
  	while(!((pEmac->EMAC_SR) & AT91C_EMAC_IDLE));
	pEmac->EMAC_CTL &= ~AT91C_EMAC_MPE;
	return (pEmac->EMAC_MAN & 0x0000ffff);
}
#endif

/*
 * .KB_C_FN_DEFINITION_START
 * void MII_GetLinkSpeed(AT91PS_EMAC pEmac)
 *  This private function determines the link speed set by the PHY.
 * .KB_C_FN_DEFINITION_END
 */
static void
MII_GetLinkSpeed(AT91PS_EMAC pEmac)
{
#if defined(BOOT_TSC) || defined(BOOT_KB920X) || defined(BOOT_CENTIPAD)
	unsigned short stat2; 
#endif
	unsigned update;
#ifdef BOOT_TSC
	unsigned sec;
	int i;
#endif
#ifdef BOOT_BWCT
	/* hardcoded link speed since we connect a switch via MII */
	update = pEmac->EMAC_CFG & ~(AT91C_EMAC_SPD | AT91C_EMAC_FD);
	update |= AT91C_EMAC_SPD;
	update |= AT91C_EMAC_FD;
#endif
#if defined(BOOT_KB920X) || defined(BOOT_CENTIPAD)
	stat2 = AT91F_MII_ReadPhy(pEmac, MII_STS2_REG);
	if (!(stat2 & MII_STS2_LINK))
		return ;
	update = pEmac->EMAC_CFG & ~(AT91C_EMAC_SPD | AT91C_EMAC_FD);
	if (stat2 & MII_STS2_100TX)
		update |= AT91C_EMAC_SPD;
	if (stat2 & MII_STS2_FDX)
		update |= AT91C_EMAC_FD;
#endif
#ifdef BOOT_TSC
	while (1) {
		for (i = 0; i < 10; i++) {
			stat2 = AT91F_MII_ReadPhy(pEmac, MII_STS_REG);
			if (stat2 & MII_STS_LINK_STAT)
				break;
			printf(".");
			sec = GetSeconds();
			while (GetSeconds() == sec)
			    continue;
		}
		if (stat2 & MII_STS_LINK_STAT)
			break;
		printf("Resetting MII...");
		AT91F_MII_WritePhy(pEmac, 0x0, 0x8000);
		while (AT91F_MII_ReadPhy(pEmac, 0x0) & 0x8000) continue;
	}
	printf("emac: link");
	stat2 = AT91F_MII_ReadPhy(pEmac, MII_SPEC_STS_REG);
	update = pEmac->EMAC_CFG & ~(AT91C_EMAC_SPD | AT91C_EMAC_FD);
	if (stat2 & (MII_SSTS_100FDX | MII_SSTS_100HDX)) {
		printf(" 100TX");
		update |= AT91C_EMAC_SPD;
	}
	if (stat2 & (MII_SSTS_100FDX | MII_SSTS_10FDX)) {
		printf(" FDX");
		update |= AT91C_EMAC_FD;
	}
	printf("\n");
#endif
	pEmac->EMAC_CFG = update;
}


/*
 * .KB_C_FN_DEFINITION_START
 * void AT91F_EmacEntry(void)
 *  This private function initializes the EMAC on the chip.
 * .KB_C_FN_DEFINITION_END
 */
static void
AT91F_EmacEntry(void)
{
	unsigned	i;
	char		*pRxPacket = (char*)RX_DATA_START;
	AT91PS_EMAC	pEmac = AT91C_BASE_EMAC;

	p_rxBD = (receive_descriptor_t*)RX_BUFFER_START;
	localPort = SWAP16(0x8002);

	for (i = 0; i < MAX_RX_PACKETS; ++i) {

		p_rxBD[i].address = (unsigned)pRxPacket;
		p_rxBD[i].size = 0;
		pRxPacket += RX_PACKET_SIZE;
	}
	
	// Set the WRAP bit at the end of the list descriptor
	p_rxBD[MAX_RX_PACKETS-1].address |= 0x02;

	if (!(pEmac->EMAC_SR & AT91C_EMAC_LINK))
		MII_GetLinkSpeed(pEmac);

	pEmac->EMAC_RBQP = (unsigned) p_rxBD;
	pEmac->EMAC_RSR  |= (AT91C_EMAC_OVR | AT91C_EMAC_REC | AT91C_EMAC_BNA);
	pEmac->EMAC_CTL  = AT91C_EMAC_TE | AT91C_EMAC_RE;

	pEmac->EMAC_TAR = (unsigned)transmitBuffer;
}	


/* ************************** GLOBAL FUNCTIONS ********************************/

/*
 * .KB_C_FN_DEFINITION_START
 * void SetServerIPAddress(unsigned address)
 *  This global function sets the IP of the TFTP download server.
 * .KB_C_FN_DEFINITION_END
 */
void
SetServerIPAddress(unsigned address)
{
	// force update in case the IP has changed
	serverMACSet = 0;

	serverIPAddr[0] = (address >> 24) & 0xFF;
	serverIPAddr[1] = (address >> 16) & 0xFF;
	serverIPAddr[2] = (address >>  8) & 0xFF;
	serverIPAddr[3] = (address >>  0) & 0xFF;

	serverIPSet = 1;
}


/*
 * .KB_C_FN_DEFINITION_START
 * void SetLocalIPAddress(unsigned address)
 *  This global function sets the IP of this module.
 * .KB_C_FN_DEFINITION_END
 */
void
SetLocalIPAddress(unsigned address)
{
	// force update in case the IP has changed
	serverMACSet = 0;

	localIPAddr[0] = (address >> 24) & 0xFF;
	localIPAddr[1] = (address >> 16) & 0xFF;
	localIPAddr[2] = (address >>  8) & 0xFF;
	localIPAddr[3] = (address >>  0) & 0xFF;

	localIPSet = 1;
}


/*
 * .KB_C_FN_DEFINITION_START
 * void TFTP_Download(unsigned address, char *filename)
 *  This global function initiates and processes a tftp download request.
 * The server IP, local IP, local MAC must be set before this function is
 * executed.
 * .KB_C_FN_DEFINITION_END
 */
void
TFTP_Download(unsigned address, char *filename)
{
	ip_header_t 	IpHeader;
	unsigned	thisSeconds;
	int		timeout;

	if ((!localMACSet) || (!localIPSet) || (!serverIPSet))
		return ;

	AT91F_EmacEntry();
	GetServerAddress();
	dlAddress = (char*)address;
	lastSize = 0;
	timeout = 10;
	thisSeconds = (GetSeconds() + 2) % 32;
	serverPort = SWAP16(69);
	++localPort;
	ackBlock = -1;

	while (timeout) {
		if (CheckForNewPacket(&IpHeader)) {
			if (ackBlock == -2)
				break;
			timeout = 10;
			thisSeconds = (GetSeconds() + 2) % 32;
		} else if (GetSeconds() == thisSeconds) {
			--timeout;
			thisSeconds = (GetSeconds() + 2) % 32;
			if (!serverMACSet)
				GetServerAddress();
			else if (ackBlock == -1)
				TFTP_RequestFile(filename);
			else {
				// Be sure to send a NAK, which is done by
				// ACKing the last block we got.
				TFTP_ACK_Data(0, ackBlock, 512);
				printf("\nNAK %u\n", ackBlock);
			}
		}
	}
	if (timeout == 0)
		printf("TFTP TIMEOUT!\n");
}
