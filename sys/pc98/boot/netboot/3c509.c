/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters.
  Date: Mar 22 1995

 This code is based heavily on David Greenman's if_ed.c driver and
  Andres Vega Garcia's if_ep.c driver.

 Copyright (C) 1993-1994, David Greenman, Martin Renters.
 Copyright (C) 1993-1995, Andres Vega Garcia.
 Copyright (C) 1995, Serge Babkin.
  This software may be used, modified, copied, distributed, and sold, in
  both source and binary form provided that the above copyright and these
  terms are retained. Under no circumstances are the authors responsible for
  the proper functioning of this software, nor do the authors assume any
  responsibility for damages incurred with its use.

3c509 support added by Serge Babkin (babkin@hq.icb.chel.su)

$FreeBSD$

***************************************************************************/

/* #define EDEBUG */

#include "netboot.h"
#include "3c509.h"

short aui;
char bnc=0, utp=0; /* for 3C509 */
unsigned short eth_nic_base;
unsigned short eth_asic_base;
unsigned short eth_base;
unsigned char  eth_tx_start;
unsigned char  eth_laar;
unsigned char  eth_flags;
unsigned char  eth_vendor;
unsigned char  eth_memsize;
unsigned char  *eth_bmem;
unsigned char  *eth_rmem;
unsigned char  *eth_node_addr;

static send_ID_sequence();
static get_eeprom_data();
static get_e();

/**************************************************************************
The following two variables are used externally
***************************************************************************/
char packet[ETHER_MAX_LEN];
int  packetlen;

/*********************** Name of driver *********************************/

char eth_driver[]="ep0";

/**************************************************************************
ETH_PROBE - Look for an adapter
***************************************************************************/
eth_probe()
{
	/* common variables */
	int i;
	/* variables for 3C509 */
	int data, j, io_base, id_port = EP_ID_PORT;
	int nisa = 0, neisa = 0;
	u_short k;
	int ep_current_tag = EP_LAST_TAG + 1;
	short *p;

	eth_vendor = VENDOR_NONE;

	/*********************************************************
			Search for 3Com 509 card
	***********************************************************/

	/* Look for the EISA boards, leave them activated */
	/* search for the first card, ignore all others */
	for(j = 1; j < 16 && eth_vendor==VENDOR_NONE ; j++) {
		io_base = (j * EP_EISA_START) | EP_EISA_W0;
		if (inw(io_base + EP_W0_MFG_ID) != MFG_ID)
			continue;

		/* we must found 0x1f if the board is EISA configurated */
		if ((inw(io_base + EP_W0_ADDRESS_CFG) & 0x1f) != 0x1f)
			continue;

		/* Reset and Enable the card */
		outb(io_base + EP_W0_CONFIG_CTRL, W0_P4_CMD_RESET_ADAPTER);
		DELAY(1000); /* we must wait at least 1 ms */
		outb(io_base + EP_W0_CONFIG_CTRL, W0_P4_CMD_ENABLE_ADAPTER);

		/*
			 * Once activated, all the registers are mapped in the range
			 * x000 - x00F, where x is the slot number.
			 */
		eth_base = j * EP_EISA_START;
		eth_vendor = VENDOR_3C509;
	}
	ep_current_tag--;

	/* Look for the ISA boards. Init and leave them actived */
	/* search for the first card, ignore all others */
	outb(id_port, 0xc0);	/* Global reset */
	DELAY(1000);
	for (i = 0; i < EP_MAX_BOARDS && eth_vendor==VENDOR_NONE; i++) {
		outb(id_port, 0);
		outb(id_port, 0);
		send_ID_sequence(id_port);

		data = get_eeprom_data(id_port, EEPROM_MFG_ID);
		if (data != MFG_ID)
			break;

		/* resolve contention using the Ethernet address */
		for (j = 0; j < 3; j++)
			data = get_eeprom_data(id_port, j);

		eth_base =
		    (get_eeprom_data(id_port, EEPROM_ADDR_CFG) & 0x1f) * 0x10 + 0x200;
		outb(id_port, ep_current_tag);	/* tags board */
		outb(id_port, ACTIVATE_ADAPTER_TO_CONFIG);
		eth_vendor = VENDOR_3C509;
		ep_current_tag--;
	}

	if(eth_vendor != VENDOR_3C509)
		goto no3c509;

	/*
	* The iobase was found and MFG_ID was 0x6d50. PROD_ID should be
	* 0x9[0-f]50
	*/
	GO_WINDOW(0);
	k = get_e(EEPROM_PROD_ID);
	if ((k & 0xf0ff) != (PROD_ID & 0xf0ff))
		goto no3c509;

	if(eth_base >= EP_EISA_START) {
		printf("3C5x9 board on EISA at 0x%x - ",eth_base);
	} else {
		printf("3C5x9 board on ISA at 0x%x - ",eth_base);
	}

	/* test for presence of connectors */
	i = inw(IS_BASE + EP_W0_CONFIG_CTRL);
	j = inw(IS_BASE + EP_W0_ADDRESS_CFG) >> 14;

	switch(j) {
		case 0:
			if(i & IS_UTP) {
				printf("10baseT\r\n");
				utp=1;
				}
			else {
				printf("10baseT not present\r\n");
				eth_vendor=VENDOR_NONE;
				goto no3c509;
				}

			break;
		case 1:
			if(i & IS_AUI)
				printf("10base5\r\n");
			else {
				printf("10base5 not present\r\n");
				eth_vendor=VENDOR_NONE;
				goto no3c509;
				}

			break;
		case 3:
			if(i & IS_BNC) {
				printf("10base2\r\n");
				bnc=1;
				}
			else {
				printf("10base2 not present\r\n");
				eth_vendor=VENDOR_NONE;
				goto no3c509;
				}

			break;
		default:
			printf("unknown connector\r\n");
			eth_vendor=VENDOR_NONE;
			goto no3c509;
		}
	/*
	* Read the station address from the eeprom
	*/
	p = (u_short *) arptable[ARP_CLIENT].node;
	for (i = 0; i < 3; i++) {
		GO_WINDOW(0);
		p[i] = htons(get_e(i));
		GO_WINDOW(2);
		outw(BASE + EP_W2_ADDR_0 + (i * 2), ntohs(p[i]));
	}

	printf("Ethernet address: ");
	for(i=0; i<5; i++) {
		printf("%b:",arptable[ARP_CLIENT].node[i]);
	}
	printf("%b\n",arptable[ARP_CLIENT].node[i]);

	eth_node_addr = arptable[ARP_CLIENT].node;
	eth_reset();
	return eth_vendor;
no3c509:
	eth_vendor = VENDOR_NONE;

	return VENDOR_NONE;
}

/**************************************************************************
ETH_RESET - Reset adapter
***************************************************************************/
eth_reset()
{
	int s, i;

	/***********************************************************
			Reset 3Com 509 card
	*************************************************************/

	if(eth_vendor != VENDOR_3C509)
		goto no3c509;

	/* stop card */
	outw(BASE + EP_COMMAND, RX_DISABLE);
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);
	outw(BASE + EP_COMMAND, TX_DISABLE);
	outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);
	outw(BASE + EP_COMMAND, RX_RESET);
	outw(BASE + EP_COMMAND, TX_RESET);
	outw(BASE + EP_COMMAND, C_INTR_LATCH);
	outw(BASE + EP_COMMAND, SET_RD_0_MASK);
	outw(BASE + EP_COMMAND, SET_INTR_MASK);
	outw(BASE + EP_COMMAND, SET_RX_FILTER);

	/*
	/* initialize card
	*/
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

	GO_WINDOW(0);

	/* Disable the card */
	outw(BASE + EP_W0_CONFIG_CTRL, 0);

	/* Configure IRQ to none */
	outw(BASE + EP_W0_RESOURCE_CFG, SET_IRQ(0));

	/* Enable the card */
	outw(BASE + EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);

	GO_WINDOW(2);

	/* Reload the ether_addr. */
	for (i = 0; i < 6; i++)
		outb(BASE + EP_W2_ADDR_0 + i, arptable[ARP_CLIENT].node[i]);

	outw(BASE + EP_COMMAND, RX_RESET);
	outw(BASE + EP_COMMAND, TX_RESET);

	/* Window 1 is operating window */
	GO_WINDOW(1);
	for (i = 0; i < 31; i++)
		inb(BASE + EP_W1_TX_STATUS);

	/* get rid of stray intr's */
	outw(BASE + EP_COMMAND, ACK_INTR | 0xff);

	outw(BASE + EP_COMMAND, SET_RD_0_MASK | S_5_INTS);

	outw(BASE + EP_COMMAND, SET_INTR_MASK | S_5_INTS);

	outw(BASE + EP_COMMAND, SET_RX_FILTER | FIL_INDIVIDUAL |
	    FIL_BRDCST);

	/* configure BNC */
	if(bnc) {
		outw(BASE + EP_COMMAND, START_TRANSCEIVER);
		DELAY(1000);
		}
	/* configure UTP */
	if(utp) {
		GO_WINDOW(4);
		outw(BASE + EP_W4_MEDIA_TYPE, ENABLE_UTP);
		GO_WINDOW(1);
		}

	/* start tranciever and receiver */
	outw(BASE + EP_COMMAND, RX_ENABLE);
	outw(BASE + EP_COMMAND, TX_ENABLE);

	/* set early threshold for minimal packet length */
	outw(BASE + EP_COMMAND, SET_RX_EARLY_THRESH | 64);

	outw(BASE + EP_COMMAND, SET_TX_START_THRESH | 16);

	return 1;
no3c509:

}

/**************************************************************************
ETH_TRANSMIT - Transmit a frame
***************************************************************************/
static const char padmap[] = {
	0, 3, 2, 1};

eth_transmit(d,t,s,p)
char *d;			/* Destination */
unsigned short t;		/* Type */
unsigned short s;		/* size */
char *p;			/* Packet */
{
	register u_int len;
	int pad;
	int status;
	unsigned char c;

	if(eth_vendor != VENDOR_3C509)
		goto no3c509;

#ifdef EDEBUG
	printf("{l=%d,t=%x}",s+14,t);
#endif

	/* swap bytes of type */
	t=(( t&0xFF )<<8) | ((t>>8) & 0xFF);

	len=s+14; /* actual length of packet */
	pad = padmap[len & 3];

	/*
	* The 3c509 automatically pads short packets to minimum ethernet length,
	* but we drop packets that are too large. Perhaps we should truncate
	* them instead?
	*/
	if (len + pad > ETHER_MAX_LEN) {
		return 0;
	}

	/* drop acknowledgements */
	while(( status=inb(BASE + EP_W1_TX_STATUS) )& TXS_COMPLETE ) {
		if(status & (TXS_UNDERRUN|TXS_MAX_COLLISION|TXS_STATUS_OVERFLOW)) {
			outw(BASE + EP_COMMAND, TX_RESET);
			outw(BASE + EP_COMMAND, TX_ENABLE);
		}

		outb(BASE + EP_W1_TX_STATUS, 0x0);
	}

	while (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
		/* no room in FIFO */
	}

	outw(BASE + EP_W1_TX_PIO_WR_1, len);
	outw(BASE + EP_W1_TX_PIO_WR_1, 0x0);	/* Second dword meaningless */

	/* write packet */
	outsw(BASE + EP_W1_TX_PIO_WR_1, d, 3);
	outsw(BASE + EP_W1_TX_PIO_WR_1, eth_node_addr, 3);
	outw(BASE + EP_W1_TX_PIO_WR_1, t);
	outsw(BASE + EP_W1_TX_PIO_WR_1, p, s / 2);
	if (s & 1)
		outb(BASE + EP_W1_TX_PIO_WR_1, *(p+s - 1));

	while (pad--)
		outb(BASE + EP_W1_TX_PIO_WR_1, 0);	/* Padding */

	/* timeout after sending */
	DELAY(1000);
	return 0;
no3c509:
}

/**************************************************************************
ETH_POLL - Wait for a frame
***************************************************************************/
eth_poll()
{
	/* common variables */
	unsigned short type = 0;
	unsigned short len;
	/* variables for 3C509 */
	struct ether_header *eh;
	int lenthisone;
	short rx_fifo2, status, cst;
	register short rx_fifo;

	if(eth_vendor!=VENDOR_3C509)
		goto no3c509;

	cst=inw(BASE + EP_STATUS);

#ifdef EDEBUG
	if(cst & 0x1FFF)
		printf("-%x-",cst);
#endif

	if( (cst & (S_RX_COMPLETE|S_RX_EARLY) )==0 ) {
		/* acknowledge  everything */
		outw(BASE + EP_COMMAND, ACK_INTR| (cst & S_5_INTS));
		outw(BASE + EP_COMMAND, C_INTR_LATCH);

		return 0;
	}

	status = inw(BASE + EP_W1_RX_STATUS);
#ifdef EDEBUG
	printf("*%x*",status);
#endif

	if (status & ERR_RX) {
		outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
		return 0;
	}

	rx_fifo = status & RX_BYTES_MASK;
	if (rx_fifo==0)
		return 0;

		/* read packet */
#ifdef EDEBUG
	printf("[l=%d",rx_fifo);
#endif
	insw(BASE + EP_W1_RX_PIO_RD_1, packet, rx_fifo / 2);
	if(rx_fifo & 1)
		packet[rx_fifo-1]=inb(BASE + EP_W1_RX_PIO_RD_1);
	packetlen=rx_fifo;

	while(1) {
		status = inw(BASE + EP_W1_RX_STATUS);
#ifdef EDEBUG
		printf("*%x*",status);
#endif
		rx_fifo = status & RX_BYTES_MASK;

		if(rx_fifo>0) {
			insw(BASE + EP_W1_RX_PIO_RD_1, packet+packetlen, rx_fifo / 2);
			if(rx_fifo & 1)
				packet[packetlen+rx_fifo-1]=inb(BASE + EP_W1_RX_PIO_RD_1);
			packetlen+=rx_fifo;
#ifdef EDEBUG
			printf("+%d",rx_fifo);
#endif
		}

		if(( status & RX_INCOMPLETE )==0) {
#ifdef EDEBUG
			printf("=%d",packetlen);
#endif
			break;
		}

		DELAY(1000);
	}

	/* acknowledge reception of packet */
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inw(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS);

	type = (packet[12]<<8) | packet[13];

#ifdef EDEBUG
	if(packet[0]+packet[1]+packet[2]+packet[3]+packet[4]+
	    packet[5] == 0xFF*6)
		printf(",t=0x%x,b]",type);
	else
		printf(",t=0x%x]",type);
#endif


	if (type == ARP) {
		struct arprequest *arpreq;
		unsigned long reqip;

		arpreq = (struct arprequest *)&packet[ETHER_HDR_LEN];

#ifdef EDEBUG
		printf("(ARP %I->%I)",ntohl(*(int*)arpreq->sipaddr),
		    ntohl(*(int*)arpreq->tipaddr));
#endif

		convert_ipaddr(&reqip, arpreq->tipaddr);
		if ((ntohs(arpreq->opcode) == ARP_REQUEST) &&
		    (reqip == arptable[ARP_CLIENT].ipaddr)) {
			arpreq->opcode = htons(ARP_REPLY);
			bcopy(arpreq->sipaddr, arpreq->tipaddr, 4);
			bcopy(arpreq->shwaddr, arpreq->thwaddr, 6);
			bcopy(arptable[ARP_CLIENT].node, arpreq->shwaddr, 6);
			convert_ipaddr(arpreq->sipaddr, &reqip);
			eth_transmit(arpreq->thwaddr, ARP, sizeof(struct arprequest),
			    arpreq);
			return(0);
		}
	} else if(type==IP) {
		struct iphdr *iph;

		iph = (struct iphdr *)&packet[ETHER_HDR_LEN];
#ifdef EDEBUG
		printf("(IP %I-%d->%I)",ntohl(*(int*)iph->src),
		    ntohs(iph->protocol),ntohl(*(int*)iph->dest));
#endif
	}

	return 1;

no3c509:
}


/*************************************************************************
	3Com 509 - specific routines
**************************************************************************/

static int
eeprom_rdy()
{
	int i;

	for (i = 0; is_eeprom_busy(IS_BASE) && i < MAX_EEPROMBUSY; i++);
	if (i >= MAX_EEPROMBUSY) {
		printf("3c509: eeprom failed to come ready.\r\n");
		return (0);
	}
	return (1);
}

/*
 * get_e: gets a 16 bits word from the EEPROM. we must have set the window
 * before
 */
static int
get_e(offset)
int offset;
{
	if (!eeprom_rdy())
		return (0xffff);
	outw(IS_BASE + EP_W0_EEPROM_COMMAND, EEPROM_CMD_RD | offset);
	if (!eeprom_rdy())
		return (0xffff);
	return (inw(IS_BASE + EP_W0_EEPROM_DATA));
}

static int
send_ID_sequence(port)
int port;
{
	int cx, al;

	for (al = 0xff, cx = 0; cx < 255; cx++) {
		outb(port, al);
		al <<= 1;
		if (al & 0x100)
			al ^= 0xcf;
	}
	return (1);
}


/*
 * We get eeprom data from the id_port given an offset into the eeprom.
 * Basically; after the ID_sequence is sent to all of the cards; they enter
 * the ID_CMD state where they will accept command requests. 0x80-0xbf loads
 * the eeprom data.  We then read the port 16 times and with every read; the
 * cards check for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle; each card
 * compares the data on the bus; if there is a difference then that card goes
 * into ID_WAIT state again). In the meantime; one bit of data is returned in
 * the AX register which is conveniently returned to us by inb().  Hence; we
 * read 16 times getting one bit of data with each read.
 */
static int
get_eeprom_data(id_port, offset)
int id_port;
int offset;
{
	int i, data = 0;
	outb(id_port, 0x80 + offset);
	DELAY(1000);
	for (i = 0; i < 16; i++)
		data = (data << 1) | (inw(id_port) & 1);
	return (data);
}

/* a surrogate */

DELAY(val)
{
	int c;

	for(c=0; c<val; c+=20) {
		twiddle();
	}
}

