
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

3c503 support added by Bill Paul (wpaul@ctr.columbia.edu) on 11/15/94
SMC8416 support added by Bill Paul (wpaul@ctr.columbia.edu) on 12/25/94
3c509 support added by Serge Babkin (babkin@hq.icb.chel.su) on 03/22/95

***************************************************************************/

/* #define EDEBUG */

#include "netboot.h"
#include "ether.h"

#ifdef INCLUDE_3C509
#	include "if_epreg.h"
#endif

extern  short aui;
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

#ifdef INCLUDE_3C509

static send_ID_sequence();
static get_eeprom_data();
static get_e();

#endif

/**************************************************************************
The following two variables are used externally
***************************************************************************/
char packet[ETH_MAX_PACKET];
int  packetlen;

/*************************************************************************
ETH_FILLNAME - Fill name of adapter in NFS structure
**************************************************************************/

eth_fillname(where)
char *where;
{
	switch(eth_vendor) {
	case VENDOR_3C509:
		where[0]='e'; where[1]='p'; where[2]='0'; where[3]=0;
		break;
	case VENDOR_WD:
	case VENDOR_NOVELL:
	case VENDOR_3COM:
		where[0]='e'; where[1]='d'; where[2]='0'; where[3]=0;
		break;
	default:
		where[0]='?'; where[1]='?'; where[2]='?'; where[3]=0;
		break;
	}
}

/**************************************************************************
ETH_PROBE - Look for an adapter
***************************************************************************/
eth_probe()
{
	/* common variables */
	int i;
#ifdef INCLUDE_3C509
	/* variables for 3C509 */
	int data, j, io_base, id_port = EP_ID_PORT;
	int nisa = 0, neisa = 0;
	u_short k;
	int ep_current_tag = EP_LAST_TAG + 1;
	short *p;
#endif
#if defined(INCLUDE_3COM) || defined(INCLUDE_WD) || defined(INCLUDE_NE)
	/* varaibles for 8390 */
	struct wd_board *brd;
	char *name;
	unsigned short chksum;
	unsigned char c;
#endif

	eth_vendor = VENDOR_NONE;

#ifdef INCLUDE_3C509

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
#endif

#if defined(INCLUDE_3COM) || defined(INCLUDE_WD) || defined(INCLUDE_NE)
#ifdef INCLUDE_WD
	/******************************************************************
		Search for WD/SMC cards
	*******************************************************************/
	for (eth_asic_base = WD_LOW_BASE; eth_asic_base <= WD_HIGH_BASE;
	    eth_asic_base += 0x20) {
		chksum = 0;
		for (i=8; i<16; i++)
			chksum += inb(i+eth_asic_base);
		if ((chksum & 0x00FF) == 0x00FF)
			break;
	}
	if (eth_asic_base <= WD_HIGH_BASE) { /* We've found a board */
		eth_vendor = VENDOR_WD;
		eth_nic_base = eth_asic_base + WD_NIC_ADDR;
		c = inb(eth_asic_base+WD_BID);	/* Get board id */
		for (brd = wd_boards; brd->name; brd++)
			if (brd->id == c) break;
		if (!brd->name) {
			printf("\r\nUnknown Ethernet type %x\r\n", c);
			return(0);	/* Unknown type */
		}
		eth_flags = brd->flags;
		eth_memsize = brd->memsize;
		eth_tx_start = 0;
		if ((c == TYPE_WD8013EP) &&
		    (inb(eth_asic_base + WD_ICR) & WD_ICR_16BIT)) {
			eth_flags = FLAG_16BIT;
			eth_memsize = MEM_16384;
		}
		if ((c & WD_SOFTCONFIG) && (!(eth_flags & FLAG_790))) {
			eth_bmem = (char *)(0x80000 |
			    ((inb(eth_asic_base + WD_MSR) & 0x3F) << 13));
		} else
			eth_bmem = (char *)WD_DEFAULT_MEM;
		if (brd->id == TYPE_SMC8216T || brd->id == TYPE_SMC8216C) {
			(unsigned int) *(eth_bmem + 8192) = (unsigned int)0;
			if ((unsigned int) *(eth_bmem + 8192)) {
				brd += 2;
				eth_memsize = brd->memsize;
			}
		}
		outb(eth_asic_base + WD_MSR, 0x80);	/* Reset */
		printf("\r\n%s base 0x%x, memory 0x%X, addr ",
		    brd->name, eth_asic_base, eth_bmem);
		for (i=0; i<6; i++) {
			printf("%b",(int)(arptable[ARP_CLIENT].node[i] =
			    inb(i+eth_asic_base+WD_LAR)));
			if (i < 5) printf (":");
		}
		if (eth_flags & FLAG_790) {
			outb(eth_asic_base+WD_MSR, WD_MSR_MENB);
			outb(eth_asic_base+0x04, (inb(eth_asic_base+0x04) |
			    0x80));
			outb(eth_asic_base+0x0B,
			    (((unsigned)eth_bmem >> 13) & 0x0F) |
			    (((unsigned)eth_bmem >> 11) & 0x40) |
			    (inb(eth_asic_base+0x0B) & 0xB0));
			outb(eth_asic_base+0x04, (inb(eth_asic_base+0x04) &
			    ~0x80));
		} else {
			outb(eth_asic_base+WD_MSR,
			    (((unsigned)eth_bmem >> 13) & 0x3F) | 0x40);
		}
		if (eth_flags & FLAG_16BIT) {
			if (eth_flags & FLAG_790) {
				eth_laar = inb(eth_asic_base + WD_LAAR);
				outb(eth_asic_base + WD_LAAR, WD_LAAR_M16EN);
				inb(0x84);
			} else {
				outb(eth_asic_base + WD_LAAR, (eth_laar =
				    WD_LAAR_M16EN | WD_LAAR_L16EN | 1));
			}
		}
		printf("\r\n");

	}
#endif
#ifdef INCLUDE_3COM
	/******************************************************************
	        Search for 3Com 3c503 if no WD/SMC cards
        *******************************************************************/
	if (eth_vendor == VENDOR_NONE) {
		eth_asic_base = _3COM_BASE + _3COM_ASIC_OFFSET;
		eth_nic_base = _3COM_BASE;
		eth_vendor = VENDOR_3COM;
		/*
		 * Note that we use the same settings for both 8 and 16 bit cards:
		 * both have an 8K bank of memory at page 1 while only the 16 bit
		 * cards have a bank at page 0.
		 */
		eth_memsize = MEM_16384;
		eth_tx_start = 32;

		/* Check our base address */

		switch(inb(eth_asic_base + _3COM_BCFR)) {
		case _3COM_BCFR_300:
			if ((int)eth_nic_base != 0x300)
				return(0);
			break;
		case _3COM_BCFR_310:
			if ((int)eth_nic_base != 0x310)
				return(0);
			break;
		case _3COM_BCFR_330:
			if ((int)eth_nic_base != 0x330)
				return(0);
			break;
		case _3COM_BCFR_350:
			if ((int)eth_nic_base != 0x350)
				return(0);
			break;
		case _3COM_BCFR_250:
			if ((int)eth_nic_base != 0x250)
				return(0);
			break;
		case _3COM_BCFR_280:
			if ((int)eth_nic_base != 0x280)
				return(0);
			break;
		case _3COM_BCFR_2A0:
			if ((int)eth_nic_base != 0x2a0)
				return(0);
			break;
		case _3COM_BCFR_2E0:
			if ((int)eth_nic_base != 0x2e0)
				return(0);
			break;
		default:
			return (0);
		}

		/* Now get the shared memory address */

		switch (inb(eth_asic_base + _3COM_PCFR)) {
		case _3COM_PCFR_DC000:
			eth_bmem = (char *)0xdc000;
			break;
		case _3COM_PCFR_D8000:
			eth_bmem = (char *)0xd8000;
			break;
		case _3COM_PCFR_CC000:
			eth_bmem = (char *)0xcc000;
			break;
		case _3COM_PCFR_C8000:
			eth_bmem = (char *)0xc8000;
			break;
		default:
			return (0);
		}

		/* Need this to make eth_poll() happy. */

		eth_rmem = eth_bmem - 0x2000;

		/* Reset NIC and ASIC */

		outb (eth_asic_base + _3COM_CR , _3COM_CR_RST | _3COM_CR_XSEL);
		outb (eth_asic_base + _3COM_CR , _3COM_CR_XSEL);

		/* Get our ethernet address */

		outb(eth_asic_base + _3COM_CR, _3COM_CR_EALO | _3COM_CR_XSEL);
		printf("\r\n3Com 3c503 base 0x%x, memory 0x%X addr ",
		    eth_nic_base, eth_bmem);
		for (i=0; i<6; i++) {
			printf("%b",(int)(arptable[ARP_CLIENT].node[i] =
			    inb(eth_nic_base+i)));
			if (i < 5) printf (":");
		}
		outb(eth_asic_base + _3COM_CR, _3COM_CR_XSEL);
		/*
		 * Initialize GA configuration register. Set bank and enable shared
		 * mem. We always use bank 1.
		 */
		outb(eth_asic_base + _3COM_GACFR, _3COM_GACFR_RSEL |
		    _3COM_GACFR_MBS0);

		outb(eth_asic_base + _3COM_VPTR2, 0xff);
		outb(eth_asic_base + _3COM_VPTR1, 0xff);
		outb(eth_asic_base + _3COM_VPTR0, 0x00);
		/*
		 * Clear memory and verify that it worked (we use only 8K)
		 */
		bzero(eth_bmem, 0x2000);
		for(i = 0; i < 0x2000; ++i)
			if (*((eth_bmem)+i)) {
				printf ("Failed to clear 3c503 shared mem.\r\n");
				return (0);
			}
		/*
		 * Initialize GA page/start/stop registers.
		 */
		outb(eth_asic_base + _3COM_PSTR, eth_tx_start);
		outb(eth_asic_base + _3COM_PSPR, eth_memsize);

		printf ("\r\n");

	}
#endif
#ifdef INCLUDE_NE
	/******************************************************************
		Search for NE1000/2000 if no WD/SMC or 3com cards
	*******************************************************************/
	if (eth_vendor == VENDOR_NONE) {
		char romdata[16], testbuf[32];
		char test[] = "NE1000/2000 memory";
		eth_bmem = (char *)0;		/* No shared memory */
		eth_asic_base = NE_BASE + NE_ASIC_OFFSET;
		eth_nic_base = NE_BASE;
		eth_vendor = VENDOR_NOVELL;
		eth_flags = FLAG_PIO;
		eth_memsize = MEM_16384;
		eth_tx_start = 32;
		c = inb(eth_asic_base + NE_RESET);
		outb(eth_asic_base + NE_RESET, c);
		inb(0x84);
		outb(eth_nic_base + D8390_P0_COMMAND, D8390_COMMAND_STP |
		    D8390_COMMAND_RD2);
		outb(eth_nic_base + D8390_P0_RCR, D8390_RCR_MON);
		outb(eth_nic_base + D8390_P0_DCR, D8390_DCR_FT1 | D8390_DCR_LS);
		outb(eth_nic_base + D8390_P0_PSTART, MEM_8192);
		outb(eth_nic_base + D8390_P0_PSTOP, MEM_16384);
		eth_pio_write(test, 8192, sizeof(test));
		eth_pio_read(8192, testbuf, sizeof(test));
		if (!bcompare(test, testbuf, sizeof(test))) {
			eth_flags |= FLAG_16BIT;
			eth_memsize = MEM_32768;
			eth_tx_start = 64;
			outb(eth_nic_base + D8390_P0_DCR, D8390_DCR_WTS |
			    D8390_DCR_FT1 | D8390_DCR_LS);
			outb(eth_nic_base + D8390_P0_PSTART, MEM_16384);
			outb(eth_nic_base + D8390_P0_PSTOP, MEM_32768);
			eth_pio_write(test, 16384, sizeof(test));
			eth_pio_read(16384, testbuf, sizeof(test));
			if (!bcompare(testbuf, test, sizeof(test))) return (0);
		}
		eth_pio_read(0, romdata, 16);
		printf("\r\nNE1000/NE2000 base 0x%x, addr ", eth_nic_base);
		for (i=0; i<6; i++) {
			printf("%b",(int)(arptable[ARP_CLIENT].node[i] = romdata[i
				+ ((eth_flags & FLAG_16BIT) ? i : 0)]));
			if (i < 5) printf (":");
		}
		printf("\r\n");
	}
	if (eth_vendor == VENDOR_NONE)
		goto no8390;

	if (eth_vendor != VENDOR_3COM) eth_rmem = eth_bmem;
	eth_node_addr = arptable[ARP_CLIENT].node;
	eth_reset();
	return(eth_vendor);
#endif /* NE */
no8390:
#endif /*8390 */

	return VENDOR_NONE;
}

/**************************************************************************
ETH_RESET - Reset adapter
***************************************************************************/
eth_reset()
{
	int s, i;

#ifdef INCLUDE_3C509

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

#endif

#if defined(INCLUDE_3COM) || defined(INCLUDE_WD) || defined(INCLUDE_NE)

	/**************************************************************
			Reset cards based on 8390 chip
	****************************************************************/

	if(eth_vendor!=VENDOR_WD && eth_vendor!=VENDOR_NOVELL
	    && eth_vendor!=VENDOR_3COM)
		goto no8390;

	if (eth_flags & FLAG_790)
		outb(eth_nic_base+D8390_P0_COMMAND,
		    D8390_COMMAND_PS0 | D8390_COMMAND_STP);
	else
		outb(eth_nic_base+D8390_P0_COMMAND,
		    D8390_COMMAND_PS0 | D8390_COMMAND_RD2 |
		    D8390_COMMAND_STP);
	if (eth_flags & FLAG_16BIT)
		outb(eth_nic_base+D8390_P0_DCR, 0x49);
	else
		outb(eth_nic_base+D8390_P0_DCR, 0x48);
	outb(eth_nic_base+D8390_P0_RBCR0, 0);
	outb(eth_nic_base+D8390_P0_RBCR1, 0);
	outb(eth_nic_base+D8390_P0_RCR, 4);	/* allow broadcast frames */
	outb(eth_nic_base+D8390_P0_TCR, 2);
	outb(eth_nic_base+D8390_P0_TPSR, eth_tx_start);
	outb(eth_nic_base+D8390_P0_PSTART, eth_tx_start + D8390_TXBUF_SIZE);
	if (eth_flags & FLAG_790) outb(eth_nic_base + 0x09, 0);
	outb(eth_nic_base+D8390_P0_PSTOP, eth_memsize);
	outb(eth_nic_base+D8390_P0_BOUND, eth_tx_start + D8390_TXBUF_SIZE);
	outb(eth_nic_base+D8390_P0_ISR, 0xFF);
	outb(eth_nic_base+D8390_P0_IMR, 0);
	if (eth_flags & FLAG_790)
		outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS1 |
		    D8390_COMMAND_STP);
	else
		outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS1 |
		    D8390_COMMAND_RD2 | D8390_COMMAND_STP);
	for (i=0; i<6; i++)
		outb(eth_nic_base+D8390_P1_PAR0+i, eth_node_addr[i]);
	for (i=0; i<6; i++)
		outb(eth_nic_base+D8390_P1_MAR0+i, 0xFF);
	outb(eth_nic_base+D8390_P1_CURR, eth_tx_start + D8390_TXBUF_SIZE+1);
	if (eth_flags & FLAG_790)
		outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS0 |
		    D8390_COMMAND_STA);
	else
		outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS0 |
		    D8390_COMMAND_RD2 | D8390_COMMAND_STA);
	outb(eth_nic_base+D8390_P0_ISR, 0xFF);
	outb(eth_nic_base+D8390_P0_TCR, 0);
#ifdef INCLUDE_3COM
	if (eth_vendor == VENDOR_3COM) {
		/*
		         * No way to tell whether or not we're supposed to use
		         * the 3Com's transceiver unless the user tells us.
		         * 'aui' should have some compile time default value
		         * which can be changed from the command menu.
		         */
		if (aui)
			outb(eth_asic_base + _3COM_CR, 0);
		else
			outb(eth_asic_base + _3COM_CR, _3COM_CR_XSEL);
	}
#endif
	return(1);
no8390:
#endif /* 8390 */
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

#ifdef INCLUDE_3C509

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
#endif /* 3C509 */

#if defined(INCLUDE_3COM) || defined(INCLUDE_WD) || defined(INCLUDE_NE)

	if(eth_vendor!=VENDOR_WD && eth_vendor!=VENDOR_NOVELL
	    && eth_vendor!=VENDOR_3COM)
		goto no8390;

#ifdef INCLUDE_3COM
	if (eth_vendor == VENDOR_3COM) {
		bcopy(d, eth_bmem, 6);                             /* dst */
		bcopy(eth_node_addr, eth_bmem+6, ETHER_ADDR_SIZE); /* src */
		*(eth_bmem+12) = t>>8;                             /* type */
		*(eth_bmem+13) = t;
		bcopy(p, eth_bmem+14, s);
		s += 14;
		while (s < ETH_MIN_PACKET) *(eth_bmem+(s++)) = 0;
	}
#endif
#ifdef INCLUDE_WD
	if (eth_vendor == VENDOR_WD) {		/* Memory interface */
		if (eth_flags & FLAG_16BIT) {
			outb(eth_asic_base + WD_LAAR, eth_laar | WD_LAAR_M16EN);
			inb(0x84);
		}
		if (eth_flags & FLAG_790) {
			outb(eth_asic_base + WD_MSR, WD_MSR_MENB);
			inb(0x84);
		}
		inb(0x84);
		bcopy(d, eth_bmem, 6);				   /* dst */
		bcopy(eth_node_addr, eth_bmem+6, ETHER_ADDR_SIZE); /* src */
		*(eth_bmem+12) = t>>8;				   /* type */
		*(eth_bmem+13) = t;
		bcopy(p, eth_bmem+14, s);
		s += 14;
		while (s < ETH_MIN_PACKET) *(eth_bmem+(s++)) = 0;
		if (eth_flags & FLAG_790) {
			outb(eth_asic_base + WD_MSR, 0);
			inb(0x84);
		}
		if (eth_flags & FLAG_16BIT) {
			outb(eth_asic_base + WD_LAAR, eth_laar & ~WD_LAAR_M16EN);
			inb(0x84);
		}
	}
#endif
#ifdef INCLUDE_NE
	if (eth_vendor == VENDOR_NOVELL) {	/* Programmed I/O */
		unsigned short type;
		type = (t >> 8) | (t << 8);
		eth_pio_write(d, eth_tx_start<<8, 6);
		eth_pio_write(eth_node_addr, (eth_tx_start<<8)+6, 6);
		eth_pio_write(&type, (eth_tx_start<<8)+12, 2);
		eth_pio_write(p, (eth_tx_start<<8)+14, s);
		s += 14;
		if (s < ETH_MIN_PACKET) s = ETH_MIN_PACKET;
	}
#endif
	twiddle();
	if (eth_flags & FLAG_790)
		outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS0 |
		    D8390_COMMAND_STA);
	else
		outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS0 |
		    D8390_COMMAND_RD2 | D8390_COMMAND_STA);
	outb(eth_nic_base+D8390_P0_TPSR, eth_tx_start);
	outb(eth_nic_base+D8390_P0_TBCR0, s);
	outb(eth_nic_base+D8390_P0_TBCR1, s>>8);
	if (eth_flags & FLAG_790)
		outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS0 |
		    D8390_COMMAND_TXP | D8390_COMMAND_STA);
	else
		outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS0 |
		    D8390_COMMAND_TXP | D8390_COMMAND_RD2 |
		    D8390_COMMAND_STA);
	return(0);

no8390:
#endif /* 8390 */
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
#ifdef INCLUDE_3C509
	struct ether_header *eh;
	int lenthisone;
	short rx_fifo2, status, cst;
	register short rx_fifo;
#endif
	/* variables for 8390 */
#if defined(INCLUDE_3COM) || defined(INCLUDE_WD) || defined(INCLUDE_NE)
	int ret = 0;
	unsigned char bound,curr,rstat;
	unsigned short pktoff;
	unsigned char *p;
	struct ringbuffer pkthdr;
#endif

#ifdef INCLUDE_3C509

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

		arpreq = (struct arprequest *)&packet[ETHER_HDR_SIZE];

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

		iph = (struct iphdr *)&packet[ETHER_HDR_SIZE];
#ifdef EDEBUG
		printf("(IP %I-%d->%I)",ntohl(*(int*)iph->src),
		    ntohs(iph->protocol),ntohl(*(int*)iph->dest));
#endif
	}

	return 1;

no3c509:
#endif /* 3C509 */
#if defined(INCLUDE_3COM) || defined(INCLUDE_WD) || defined(INCLUDE_NE)

	if(eth_vendor!=VENDOR_WD && eth_vendor!=VENDOR_NOVELL
	    && eth_vendor!=VENDOR_3COM)
		goto no8390;

	rstat = inb(eth_nic_base+D8390_P0_RSR);
	if (rstat & D8390_RSTAT_OVER) {
		eth_reset();
		return(0);
	}
	if (!(rstat & D8390_RSTAT_PRX)) return(0);
	bound = inb(eth_nic_base+D8390_P0_BOUND)+1;
	if (bound == eth_memsize) bound = eth_tx_start + D8390_TXBUF_SIZE;
	outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS1);
	curr = inb(eth_nic_base+D8390_P1_CURR);
	outb(eth_nic_base+D8390_P0_COMMAND, D8390_COMMAND_PS0);
	if (curr == eth_memsize) curr=eth_tx_start + D8390_TXBUF_SIZE;
	if (curr == bound) return(0);
	if (eth_vendor == VENDOR_WD) {
		if (eth_flags & FLAG_16BIT) {
			outb(eth_asic_base + WD_LAAR, eth_laar | WD_LAAR_M16EN);
			inb(0x84);
		}
		if (eth_flags & FLAG_790) {
			outb(eth_asic_base + WD_MSR, WD_MSR_MENB);
			inb(0x84);
		}
		inb(0x84);
	}
	pktoff = (bound << 8);
	if (eth_flags & FLAG_PIO)
		eth_pio_read(pktoff, &pkthdr, 4);
	else
		bcopy(eth_rmem + pktoff, &pkthdr, 4);
	len = pkthdr.len - 4; /* sub CRC */
	pktoff += 4;
	if (len > 1514) len = 1514;
	bound = pkthdr.bound;		/* New bound ptr */
	if ( (pkthdr.status & D8390_RSTAT_PRX) && (len > 14) && (len < 1518)) {
		p = packet;
		packetlen = len;
		len = (eth_memsize << 8) - pktoff;
		if (packetlen > len) {		/* We have a wrap-around */
			if (eth_flags & FLAG_PIO)
				eth_pio_read(pktoff, p, len);
			else
				bcopy(eth_rmem + pktoff, p, len);
			pktoff = (eth_tx_start + D8390_TXBUF_SIZE) << 8;
			p += len;
			packetlen -= len;
		}
		if (eth_flags & FLAG_PIO)
			eth_pio_read(pktoff, p, packetlen);
		else
			bcopy(eth_rmem + pktoff, p, packetlen);

		type = (packet[12]<<8) | packet[13];
		ret = 1;
	}
	if (eth_vendor == VENDOR_WD) {
		if (eth_flags & FLAG_790) {
			outb(eth_asic_base + WD_MSR, 0);
			inb(0x84);
		}
		if (eth_flags & FLAG_16BIT) {
			outb(eth_asic_base + WD_LAAR, eth_laar &
			    ~WD_LAAR_M16EN);
			inb(0x84);
		}
		inb(0x84);
	}
	if (bound == (eth_tx_start + D8390_TXBUF_SIZE))
		bound = eth_memsize;
	outb(eth_nic_base+D8390_P0_BOUND, bound-1);
	if (ret && (type == ARP)) {
		struct arprequest *arpreq;
		unsigned long reqip;
		arpreq = (struct arprequest *)&packet[ETHER_HDR_SIZE];
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
	}
	return(ret);
no8390:
#endif /* 8390 */
}

#ifdef INCLUDE_NE
/**************************************************************************
NE1000/NE2000 Support Routines
***************************************************************************/

/* inw and outw are not needed more - standard version of them is used */

/**************************************************************************
ETH_PIO_READ - Read a frame via Programmed I/O
***************************************************************************/
eth_pio_read(src, dst, cnt, init)
unsigned short src;
unsigned char *dst;
unsigned short cnt;
int init;
{
	if (cnt & 1) cnt++;
	outb(eth_nic_base + D8390_P0_COMMAND, D8390_COMMAND_RD2 |
	    D8390_COMMAND_STA);
	outb(eth_nic_base + D8390_P0_RBCR0, cnt);
	outb(eth_nic_base + D8390_P0_RBCR1, cnt>>8);
	outb(eth_nic_base + D8390_P0_RSAR0, src);
	outb(eth_nic_base + D8390_P0_RSAR1, src>>8);
	outb(eth_nic_base + D8390_P0_COMMAND, D8390_COMMAND_RD0 |
	    D8390_COMMAND_STA);
	if (eth_flags & FLAG_16BIT) {
		while (cnt) {
			*((unsigned short *)dst) = inw(eth_asic_base + NE_DATA);
			dst += 2;
			cnt -= 2;
		}
	}
	else {
		while (cnt--)
			*(dst++) = inb(eth_asic_base + NE_DATA);
	}
}

/**************************************************************************
ETH_PIO_WRITE - Write a frame via Programmed I/O
***************************************************************************/
eth_pio_write(src, dst, cnt, init)
unsigned char *src;
unsigned short dst;
unsigned short cnt;
int init;
{
	outb(eth_nic_base + D8390_P0_COMMAND, D8390_COMMAND_RD2 |
	    D8390_COMMAND_STA);
	outb(eth_nic_base + D8390_P0_ISR, D8390_ISR_RDC);
	outb(eth_nic_base + D8390_P0_RBCR0, cnt);
	outb(eth_nic_base + D8390_P0_RBCR1, cnt>>8);
	outb(eth_nic_base + D8390_P0_RSAR0, dst);
	outb(eth_nic_base + D8390_P0_RSAR1, dst>>8);
	outb(eth_nic_base + D8390_P0_COMMAND, D8390_COMMAND_RD1 |
	    D8390_COMMAND_STA);
	if (eth_flags & FLAG_16BIT) {
		if (cnt & 1) cnt++;		/* Round up */
		while (cnt) {
			outw(eth_asic_base + NE_DATA, *((unsigned short *)src));
			src += 2;
			cnt -= 2;
		}
	}
	else {
		while (cnt--)
			outb(eth_asic_base + NE_DATA, *(src++));
	}
	while((inb(eth_nic_base + D8390_P0_ISR) & D8390_ISR_RDC)
	    != D8390_ISR_RDC);
}
#else
/**************************************************************************
ETH_PIO_READ - Dummy routine when NE2000 not compiled in
***************************************************************************/
eth_pio_read() {
}
#endif

#ifdef INCLUDE_3C509
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

#endif

