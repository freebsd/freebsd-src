
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


**************************************************************************/

#include "netboot.h"
#include "ether.h"

unsigned short eth_nic_base;
unsigned short eth_asic_base;
unsigned char  eth_tx_start;
unsigned char  eth_laar;
unsigned char  eth_flags;
unsigned char  eth_vendor;
unsigned char  eth_memsize;
unsigned char  *eth_bmem;
unsigned char  *eth_node_addr;

/**************************************************************************
The following two variables are used externally
**************************************************************************/
char packet[ETH_MAX_PACKET];
int  packetlen;

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
		outb(eth_asic_base + WD_MSR, 0x80);	/* Reset */
		printf("\r\n%s base 0x%x, memory 0x%X, addr ",
			brd->name, eth_asic_base, eth_bmem);
		for (i=0; i<6; i++)
			printhb((int)(arptable[ARP_CLIENT].node[i] =
				inb(i+eth_asic_base+WD_LAR)));
		if (eth_flags & FLAG_790) {
			outb(eth_asic_base+WD_MSR, WD_MSR_MENB);
			outb(eth_asic_base+0x04, (inb(eth_asic_base+0x04) |
				0x80));
			outb(eth_asic_base+0x0B,
				(((unsigned)eth_bmem >> 13) & 0x0F) |
				(((unsigned)eth_bmem >> 11) & 0x40) |
				(inb(eth_asic_base+0x0B) & 0x0B));
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
#ifdef INCLUDE_NE
	/******************************************************************
	Search for NE1000/2000 if no WD/SMC cards
	******************************************************************/
	if (eth_vendor != VENDOR_WD) {
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
			if (!bcompare(testbuf, test, sizeof(test))) return(0);
		}
		eth_pio_read(0, romdata, 16);
		printf("\r\nNE1000/NE2000 base 0x%x, addr ", eth_nic_base);
		for (i=0; i<6; i++)
			printhb((int)(arptable[ARP_CLIENT].node[i] = romdata[i
				+ ((eth_flags & FLAG_16BIT) ? i : 0)]));
		printf("\r\n");
	}
#endif
	if (eth_vendor == VENDOR_NONE) return(0);

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
		bcopy(eth_bmem + pktoff, &pkthdr, 4);
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
				bcopy(eth_bmem + pktoff, p, len);
			pktoff = (eth_tx_start + D8390_TXBUF_SIZE) << 8;
			p += len;
			packetlen -= len;
		}
		if (eth_flags & FLAG_PIO)
			eth_pio_read(pktoff, p, packetlen);
		else
			bcopy(eth_bmem + pktoff, p, packetlen);

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
}

#ifdef INCLUDE_NE
/**************************************************************************
NE1000/NE2000 Support Routines
**************************************************************************/
static inline unsigned short inw(unsigned short a)
{
	unsigned short d;
        asm volatile( "inw %1, %0" : "=a" (d) : "d" (a));
        return d;
}

static inline void outw(unsigned short a, unsigned short d)
{
	asm volatile( "outw %0, %1" : : "a" (d), "d" (a));
}

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
