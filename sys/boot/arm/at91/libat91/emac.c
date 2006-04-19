/******************************************************************************
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
 *****************************************************************************/

#include "at91rm9200.h"
#include "emac.h"
#include "p_string.h"
#include "at91rm9200_lowlevel.h"
#include "lib.h"

/******************************* GLOBALS *************************************/

/*********************** PRIVATE FUNCTIONS/DATA ******************************/

static unsigned localMACSet, serverMACSet, MAC_init;
static unsigned char localMACAddr[6], serverMACAddr[6];
static unsigned localIPAddr, serverIPAddr;
static unsigned short	serverPort, localPort;
static int	ackBlock;

static unsigned	lastAddress, lastSize;
static char *dlAddress;

static unsigned transmitBuffer[1024 / sizeof(unsigned)];
static unsigned tftpSendPacket[256 / sizeof(unsigned)];

receive_descriptor_t *p_rxBD;


/*
 * .KB_C_FN_DEFINITION_START
 * unsigned short IP_checksum(unsigned short *p, int len)
 *  This private function calculates the IP checksum for various headers.
 * .KB_C_FN_DEFINITION_END
 */
static unsigned short
IP_checksum(void *cp, int len)
{
	unsigned	i, t;
	unsigned short *p = (unsigned short *)cp;

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
	p_memcpy((char*)p_ARP->src_mac, (char*)localMACAddr, 6);

	p_ARP->frame_type = SWAP16(PROTOCOL_ARP);
	p_ARP->hard_type  = SWAP16(1);
	p_ARP->prot_type  = SWAP16(PROTOCOL_IP);
	p_ARP->hard_size  = 6;
	p_ARP->prot_size  = 4;
	p_ARP->operation  = SWAP16(ARP_REQUEST);

	p_memcpy((char*)p_ARP->sender_mac, (char*)localMACAddr, 6);
	p_memcpy((char*)p_ARP->sender_ip, (char*)localIPAddr, 4);
	p_memset((char*)p_ARP->target_mac, 0, 6);
	p_memcpy((char*)p_ARP->target_ip, (char*)serverIPAddr, 4);

	// wait until transmit is available
	while (!(*AT91C_EMAC_TSR & AT91C_EMAC_BNQ)) ;

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

	p_memcpy((char*)macHdr->dest_mac, (char*)serverMACAddr, 6);
	p_memcpy((char*)macHdr->src_mac, (char*)localMACAddr, 6);

	macHdr->proto_mac = SWAP16(PROTOCOL_IP);

	ipHdr = &macHdr->iphdr;

	ipHdr->ip_v_hl = 0x45;
	ipHdr->ip_tos = 0;
	ipHdr->ip_len = SWAP16(28 + tftpLength);
	ipHdr->ip_id = 0;
	ipHdr->ip_off = SWAP16(0x4000);
	ipHdr->ip_ttl = 64;
	ipHdr->ip_p = PROTOCOL_UDP;
	ipHdr->ip_sum = 0;

	p_memcpy((char*)ipHdr->ip_src, (char*)localIPAddr, 4);
	p_memcpy((char*)ipHdr->ip_dst, (char*)serverIPAddr, 4);

	ipHdr->ip_sum = SWAP16(IP_checksum(ipHdr, 20));

	udpHdr = (udp_header_t*)(ipHdr + 1);

	udpHdr->src_port  = localPort;
	udpHdr->dst_port  = serverPort;
	udpHdr->udp_len   = SWAP16(8 + tftpLength);
	udpHdr->udp_cksum = 0;

	p_memcpy((char*)udpHdr+8, tftpData, tftpLength);

	t_checksum = IP_checksum((char *)ipHdr + 12, (16 + tftpLength));

	t_checksum = (~t_checksum) & 0xFFFF;
	t_checksum += 25 + tftpLength;

	t_checksum = (t_checksum & 0xffff) + (t_checksum >> 16);
	t_checksum = (~t_checksum) & 0xFFFF;

	udpHdr->udp_cksum = SWAP16(t_checksum);

	while (!(*AT91C_EMAC_TSR & AT91C_EMAC_BNQ)) ;

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

	tftpHeader.opcode = SWAP16(TFTP_RRQ_OPCODE);

	cPtr = (char*)&(tftpHeader.block_num);

	ePtr = p_strcpy(cPtr, filename);
	mPtr = p_strcpy(ePtr, "octet");

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
TFTP_ACK_Data(char *data, unsigned short block_num, unsigned short len)
{
	tftp_header_t	tftpHeader;

	if (block_num == SWAP16(ackBlock + 1)) {
		++ackBlock;
		p_memcpy(dlAddress, data, len);
		dlAddress += len;
		lastSize += len;
	}

	tftpHeader.opcode = SWAP16(TFTP_ACK_OPCODE);
	tftpHeader.block_num = block_num;
	Send_TFTP_Packet((char*)&tftpHeader, 4);

	if (len < 512)
		ackBlock = -2;
}


/*
 * .KB_C_FN_DEFINITION_START
 * void CheckForNewPacket(ip_header_t *pHeader)
 *  This private function polls for received ethernet packets and handles
 * any here.
 * .KB_C_FN_DEFINITION_END
 */
static void
CheckForNewPacket(void)
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
		return ;
						
	process = i;
		
	pFrameType = (unsigned short *) ((p_rxBD[i].address & 0xFFFFFFFC) + 12);
	pData      = (char *)(p_rxBD[i].address & 0xFFFFFFFC);

	switch (SWAP16(*pFrameType)) {

	case PROTOCOL_ARP:
		p_ARP = (arp_header_t*)pData;

		i = SWAP16(p_ARP->operation);
		if (i == ARP_REPLY) {

			// check if new server info is available
			if ((!serverMACSet) &&
				(!(p_memcmp((char*)p_ARP->sender_ip,
					(char*)serverIPAddr, 4)))) {

				serverMACSet = 1;

				p_memcpy((char*)serverMACAddr,
					(char*)p_ARP->sender_mac, 6);
			}
		} else if (i == ARP_REQUEST) {

			// ARP REPLY operation
			p_ARP->operation =  SWAP16(ARP_REPLY);

			// Swap the src/dst MAC addr
			p_memcpy(p_ARP->dest_mac, p_ARP->src_mac, 6);
			p_memcpy(p_ARP->src_mac, localMACAddr, 6);
			
			// Do IP and MAC addr at same time.
			p_memcpy(p_ARP->target_mac, p_ARP->sender_mac, 10);
			p_memcpy(p_ARP->sender_mac, localMACAddr, 6);
			p_memcpy(p_ARP->sender_ip, (char *)&localIPAddr, 4);

			if (!(*AT91C_EMAC_TSR & AT91C_EMAC_BNQ))
				break;

		  	*AT91C_EMAC_TSR |= AT91C_EMAC_COMP;
			*AT91C_EMAC_TAR = (unsigned)pData;
 			*AT91C_EMAC_TCR = 0x40;
		}
	break;
		
	case PROTOCOL_IP:
		pIpHeader = (ip_header_t*)(pData + 14);
		switch(pIpHeader->ip_p) {
		case PROTOCOL_UDP:
		{
			udp_header_t	*udpHdr;
			tftp_header_t	*tftpHdr;

			udpHdr = (udp_header_t*)((char*)pIpHeader+20);
			tftpHdr = (tftp_header_t*)((char*)udpHdr + 8);

			if (udpHdr->dst_port != localPort)
				break;

			if (tftpHdr->opcode != SWAP16(TFTP_DATA_OPCODE))
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
				tftpHdr->block_num,
				SWAP16(udpHdr->udp_len) - 12);
		}
		break;

		default:
			break;
		}
	break;
						
	default:
		break;
	}
	p_rxBD[process].address &= ~0x01;
}


/*
 * .KB_C_FN_DEFINITION_START
 * unsigned short AT91F_MII_ReadPhy (AT91PS_EMAC pEmac, unsigned char addr)
 *  This private function reads the PHY device.
 * .KB_C_FN_DEFINITION_END
 */
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


/*
 * .KB_C_FN_DEFINITION_START
 * void MII_GetLinkSpeed(AT91PS_EMAC pEmac)
 *  This private function determines the link speed set by the PHY.
 * .KB_C_FN_DEFINITION_END
 */
static void
MII_GetLinkSpeed(AT91PS_EMAC pEmac)
{
	unsigned short stat2;
	unsigned update = 0;

	stat2 = AT91F_MII_ReadPhy(pEmac, MII_STS2_REG);

	if (!(stat2 & 0x400)) {
		return ;

	} else if (stat2 & 0x4000) {

		update |= AT91C_EMAC_SPD;

		if (stat2 & 0x200) {
			update |= AT91C_EMAC_FD;
		}

	} else if (stat2 & 0x200) {
		update |= AT91C_EMAC_FD;
	}

	pEmac->EMAC_CFG =
		(pEmac->EMAC_CFG & ~(AT91C_EMAC_SPD | AT91C_EMAC_FD)) | update;
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

	for (i = 0; i < MAX_RX_PACKETS; ++i) {
		p_rxBD[i].address = (unsigned)pRxPacket;
		p_rxBD[i].size = 0;
		pRxPacket += RX_PACKET_SIZE;
	}
	
	// Set the WRAP bit at the end of the list descriptor
	p_rxBD[MAX_RX_PACKETS-1].address |= 0x02;

	pEmac->EMAC_CTL  = 0;

	if(!(pEmac->EMAC_SR & AT91C_EMAC_LINK))
		MII_GetLinkSpeed(pEmac);

	// the sequence write EMAC_SA1L and write EMAC_SA1H must be respected
	pEmac->EMAC_SA1L = ((unsigned)localMACAddr[2] << 24) | 
	    ((unsigned)localMACAddr[3] << 16) | ((int)localMACAddr[4] << 8) |
	    localMACAddr[5];
	pEmac->EMAC_SA1H = ((unsigned)localMACAddr[0] << 8) | localMACAddr[1];

	pEmac->EMAC_RBQP = (unsigned) p_rxBD;
	pEmac->EMAC_RSR  |= (AT91C_EMAC_OVR | AT91C_EMAC_REC | AT91C_EMAC_BNA);
	pEmac->EMAC_CFG  |= AT91C_EMAC_CAF;
	pEmac->EMAC_CFG  = (pEmac->EMAC_CFG & ~(AT91C_EMAC_CLK)) |
		AT91C_EMAC_CLK_HCLK_32;
	pEmac->EMAC_CTL  |= (AT91C_EMAC_TE | AT91C_EMAC_RE);

	pEmac->EMAC_TAR = (unsigned)transmitBuffer;
}	


/* ************************** GLOBAL FUNCTIONS ********************************/


/*
 * .KB_C_FN_DEFINITION_START
 * void SetMACAddress(unsigned low_address, unsigned high_address)
 *  This global function sets the MAC address.  low_address is the first
 * four bytes while high_address is the last 2 bytes of the 48-bit value.
 * .KB_C_FN_DEFINITION_END
 */
void
SetMACAddress(unsigned low_address, unsigned high_address)
{

	AT91PS_EMAC	pEmac = AT91C_BASE_EMAC;
	AT91PS_PMC	pPMC = AT91C_BASE_PMC;

	/* enable the peripheral clock before using EMAC */
	pPMC->PMC_PCER = ((unsigned) 1 << AT91C_ID_EMAC);

	pEmac->EMAC_SA1L = low_address;
	pEmac->EMAC_SA1H = (high_address & 0x0000ffff);

	localMACAddr[0] = (low_address >>  0) & 0xFF;
	localMACAddr[1] = (low_address >>  8) & 0xFF;
	localMACAddr[2] = (low_address >> 16) & 0xFF;
	localMACAddr[3] = (low_address >> 24) & 0xFF;
	localMACAddr[4] = (high_address >> 0) & 0xFF;
	localMACAddr[5] = (high_address >> 8) & 0xFF;

	localMACSet = 1;

	// low_address  & 0x000000ff = first byte in address
	// low_address  & 0x0000ff00 = next
	// low_address  & 0x00ff0000 = next
	// low_address  & 0xff000000 = next
	// high_address & 0x000000ff = next
	// high_address & 0x0000ff00 = last byte in address
}


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
	serverIPAddr = address;
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
	localIPAddr = address;
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
	unsigned	thisSeconds, running, state;
	int		timeout, tickUpdate;

	if (!address) {
		// report last transfer information
		printf("Last tftp transfer info --\r\n"
		  "address: 0x%x\r\n"
		  "   size: 0x%x\r\n", lastAddress, lastSize);
		return ;
	}

	if ((!localMACSet) || (!localIPAddr) || (!serverIPAddr))
		return ;

	if (!MAC_init) {
		AT91C_BASE_PMC->PMC_PCER = 1u << AT91C_ID_EMAC;

		AT91C_BASE_PIOA->PIO_ASR =
			AT91C_PA14_ERXER |
			AT91C_PA12_ERX0 |
			AT91C_PA13_ERX1 |
			AT91C_PA8_ETXEN |
			AT91C_PA16_EMDIO |
			AT91C_PA9_ETX0 |
			AT91C_PA10_ETX1 |
			AT91C_PA11_ECRS_ECRSDV |
			AT91C_PA15_EMDC |
			AT91C_PA7_ETXCK_EREFCK;
		AT91C_BASE_PIOA->PIO_BSR = 0;
		AT91C_BASE_PIOA->PIO_PDR =
			AT91C_PA14_ERXER |
			AT91C_PA12_ERX0 |
			AT91C_PA13_ERX1 |
			AT91C_PA8_ETXEN |
			AT91C_PA16_EMDIO |
			AT91C_PA9_ETX0 |
			AT91C_PA10_ETX1 |
			AT91C_PA11_ECRS_ECRSDV |
			AT91C_PA15_EMDC |
			AT91C_PA7_ETXCK_EREFCK;
		AT91C_BASE_PIOB->PIO_ASR = 0;
		AT91C_BASE_PIOB->PIO_BSR =
			AT91C_PB12_ETX2 |
			AT91C_PB13_ETX3 |
			AT91C_PB14_ETXER |
			AT91C_PB15_ERX2 |
			AT91C_PB16_ERX3 |
			AT91C_PB17_ERXDV |
			AT91C_PB18_ECOL |
			AT91C_PB19_ERXCK;
		AT91C_BASE_PIOB->PIO_PDR =
			AT91C_PB12_ETX2 |
			AT91C_PB13_ETX3 |
			AT91C_PB14_ETXER |
			AT91C_PB15_ERX2 |
			AT91C_PB16_ERX3 |
			AT91C_PB17_ERXDV |
			AT91C_PB18_ECOL |
			AT91C_PB19_ERXCK;
		MAC_init = 1;
	}

	AT91F_EmacEntry();

	GetServerAddress();
	lastAddress = address;
	dlAddress = (char*)address;
	lastSize = 0;
	running = 1;
	state = TFTP_WAITING_SERVER_MAC;
	timeout = 10;
	thisSeconds = GetSeconds();
	serverPort = SWAP16(69);
	localPort++;		/* In network byte order, but who cares */
	ackBlock = -1;

	while (running && timeout) {

		CheckForNewPacket();

		tickUpdate = 0;

		if (thisSeconds != GetSeconds()) {
			tickUpdate = 1;
			--timeout;
			thisSeconds = GetSeconds();
		}

		switch (state) {

			case TFTP_WAITING_SERVER_MAC:
				if (serverMACSet) {
					state = TFTP_SEND_REQUEST;
					break;
				}

				if (tickUpdate)
					GetServerAddress();
				break;

			case TFTP_SEND_REQUEST:
				// send request for file
				if (ackBlock != -1) {
					state = TFTP_GET_DATA;
					break;
				}

				if (tickUpdate)
					TFTP_RequestFile(filename);
				break;

			case TFTP_GET_DATA:
				// receiving data
				if (ackBlock == -2) {
					state = TFTP_COMPLETE;
					break;
				}
				break;

			case TFTP_COMPLETE:
			default:
				running = 0;
				break;
		}
	}
}


/*
 * .KB_C_FN_DEFINITION_START
 * void EMAC_Init(void)
 *  This global function initializes variables used in tftp transfers.
 * .KB_C_FN_DEFINITION_END
 */
void
EMAC_Init(void)
{
	p_rxBD = (receive_descriptor_t*)RX_BUFFER_START;
	localMACSet = 0;
	serverMACSet = 0;
	localPort = SWAP16(0x8002);
	lastAddress = 0;
	lastSize = 0;
	MAC_init = 0;
}
