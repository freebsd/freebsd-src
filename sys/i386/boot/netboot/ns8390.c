
/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: May/94

 This code is based heavily on David Greenman's if_ed.c driver

 Copyright (C) 1993-1994, David Greenman, Martin Renters.
  This software may be used, modified, copied, distributed, and sold, in
  both source and binary form provided that the above copyright and these
  terms are retained. Under no circumstances are the authors responsible for
  the proper functioning of this software, nor do the authors assume any
  responsibility for damages incurred with its use.

3c503 support added by Bill Paul (wpaul@ctr.columbia.edu) on 11/15/94
SMC8416 support added by Bill Paul (wpaul@ctr.columbia.edu) on 12/25/94

**************************************************************************/

#include "netboot.h"
#include "ns8390.h"

#ifdef _3COM_USE_AUI
short aui=1;
#else
short aui=0;
#endif

unsigned short eth_nic_base;
unsigned short eth_asic_base;
unsigned char  eth_tx_start;
unsigned char  eth_laar;
unsigned char  eth_flags;
unsigned char  eth_vendor;
unsigned char  eth_memsize;
unsigned char  *eth_bmem;
unsigned char  *eth_rmem;
unsigned char  *eth_node_addr;

/**************************************************************************
The following two variables are used externally
**************************************************************************/
char eth_driver[] = "ed0";
char packet[ETHER_MAX_LEN];
int  packetlen;

#ifdef	INCLUDE_NE
static unsigned short ne_base_list[]= {
#ifdef	NE_BASE
	NE_BASE,
#endif
	0xff80, 0xff40, 0xff00, 0xfec0,
	0x280, 0x300, 0
};
#endif
/**************************************************************************
ETH_PROBE - Look for an adapter
**************************************************************************/
eth_probe()
{
	int i;
	struct wd_board *brd;
	char *name;
	unsigned short chksum;
	unsigned char c;

	eth_vendor = VENDOR_NONE;

#ifdef INCLUDE_WD
	/******************************************************************
	Search for WD/SMC cards
	******************************************************************/
	for (eth_asic_base = WD_LOW_BASE; eth_asic_base <= WD_HIGH_BASE;
		eth_asic_base += 0x20) {
		chksum = 0;
		/* Check for WD/SMC card by checking ethernet address */
		if (inb(eth_asic_base+8) != 0) continue;
		if (inb(eth_asic_base+9) != 0) continue;
		if (inb(eth_asic_base+10) != 0xC0) continue;
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
		goto found_board;
	}
#endif
#ifdef INCLUDE_3COM
        /******************************************************************
        Search for 3Com 3c503 if no WD/SMC cards
        ******************************************************************/
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

		goto found_board;

        }
#endif
#ifdef INCLUDE_NE
	/******************************************************************
	Search for NE1000/2000 if no WD/SMC or 3com cards
	******************************************************************/
	if (eth_vendor == VENDOR_NONE) {
		char romdata[16], testbuf[32];
		char test[] = "NE1000/2000 memory";
		unsigned short *tent_base=ne_base_list;
		eth_bmem = (char *)0;		/* No shared memory */
ne_again:
		eth_asic_base = *tent_base + NE_ASIC_OFFSET;
		eth_nic_base = *tent_base;

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
			if (!bcompare(testbuf, test, sizeof(test)))
				if (*++tent_base)
					goto ne_again;
				else
					return (0);
		}
		eth_pio_read(0, romdata, 16);
		printf("\r\nNE1000/NE2000 base 0x%x, addr ", eth_nic_base);
		for (i=0; i<6; i++) {
			printf("%b",(int)(arptable[ARP_CLIENT].node[i] = romdata[i
				+ ((eth_flags & FLAG_16BIT) ? i : 0)]));
			if (i < 5) printf (":");
		}
		goto found_board;
	}
#endif
found_board:
	printf("\r\n");
	if (eth_vendor == VENDOR_NONE) return(0);

        if (eth_vendor != VENDOR_3COM) eth_rmem = eth_bmem;
	eth_node_addr = arptable[ARP_CLIENT].node;
	eth_reset();
	return(eth_vendor);
}

/**************************************************************************
ETH_RESET - Reset adapter
**************************************************************************/
eth_reset()
{
	int i;
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
}

/**************************************************************************
ETH_TRANSMIT - Transmit a frame
**************************************************************************/
eth_transmit(d,t,s,p)
	char *d;			/* Destination */
	unsigned short t;		/* Type */
	unsigned short s;		/* size */
	char *p;			/* Packet */
{
	unsigned char c;
#ifdef INCLUDE_3COM
        if (eth_vendor == VENDOR_3COM) {
                bcopy(d, eth_bmem, 6);                             /* dst */
                bcopy(eth_node_addr, eth_bmem+6, ETHER_ADDR_SIZE); /* src */
                *(eth_bmem+12) = t>>8;                             /* type */
                *(eth_bmem+13) = t;
                bcopy(p, eth_bmem+14, s);
                s += 14;
                while (s < ETHER_MIN_LAN) *(eth_bmem+(s++)) = 0;
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
		bcopy(eth_node_addr, eth_bmem+6, ETHER_ADDR_LEN);  /* src */
		*(eth_bmem+12) = t>>8;				   /* type */
		*(eth_bmem+13) = t;
		bcopy(p, eth_bmem+14, s);
		s += 14;
		while (s < ETHER_MIN_LEN) *(eth_bmem+(s++)) = 0;
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
		if (s < ETHER_MIN_LEN) s = ETHER_MIN_LEN;
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
}

/**************************************************************************
ETH_POLL - Wait for a frame
**************************************************************************/
eth_poll()
{
	int ret = 0;
	unsigned short type = 0;
	unsigned char bound,curr,rstat;
	unsigned short len;
	unsigned short pktoff;
	unsigned char *p;
	struct ringbuffer pkthdr;
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
		arpreq = (struct arprequest *)&packet[ETHER_HDR_LEN];
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
}

#ifdef INCLUDE_NE
/**************************************************************************
ETH_PIO_READ - Read a frame via Programmed I/O
**************************************************************************/
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
**************************************************************************/
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
**************************************************************************/
eth_pio_read() {}
#endif
