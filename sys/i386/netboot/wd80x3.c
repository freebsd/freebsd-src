
/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters
  Date: Dec/93

**************************************************************************/

#include "netboot.h"

unsigned short we_base;
unsigned char *we_bmem;

#define WE_LOW_BASE	0x200
#define WE_HIGH_BASE	0x3e0
#define WE_DEFAULT_MEM	0xD0000
#define WE_MIN_PACKET	64
#define WE_TXBUF_SIZE	6
#define WE_RXBUF_END	32

#define WE_MSR		0x00
#define WE_ICR		0x01
#define WE_IAR		0x02
#define WE_BIO		0x03
#define WE_IRR		0x04
#define WE_LAAR		0x05
#define WE_IJR		0x06
#define WE_GP2		0x07
#define WE_LAR		0x08
#define WE_BID		0x0E
#define WE_P0_COMMAND	0x10
#define WE_P0_PSTART	0x11
#define WE_P0_PSTOP	0x12
#define WE_P0_BOUND	0x13
#define WE_P0_TSR	0x14
#define	WE_P0_TPSR	0x14
#define WE_P0_TBCR0	0x15
#define WE_P0_TBCR1	0x16
#define WE_P0_ISR	0x17
#define WE_P0_RBCR0	0x1A
#define WE_P0_RBCR1	0x1B
#define WE_P0_RSR	0x1C
#define WE_P0_RCR	0x1C
#define WE_P0_TCR	0x1D
#define WE_P0_DCR	0x1E
#define WE_P0_IMR	0x1F
#define WE_P1_COMMAND	0x10
#define WE_P1_PAR0	0x11
#define WE_P1_PAR1	0x12
#define WE_P1_PAR2	0x13
#define WE_P1_PAR3	0x14
#define WE_P1_PAR4	0x15
#define WE_P1_PAR5	0x16
#define WE_P1_CURR	0x17
#define WE_P1_MAR0	0x18

#define WE_COMMAND_PS0	0x0		/* Page 0 select */
#define WE_COMMAND_PS1	0x40		/* Page 1 select */
#define WE_COMMAND_PS2	0x80		/* Page 2 select */
#define WE_COMMAND_TXP	0x04		/* transmit packet */
#define WE_COMMAND_STA	0x02		/* start */
#define WE_COMMAND_STP	0x01		/* stop */

#define WE_ISR_PRX	0x01		/* successful recv */
#define WE_ISR_PTX	0x02		/* successful xmit */
#define WE_ISR_RXE	0x04		/* receive error */
#define WE_ISR_TXE	0x08		/* transmit error */
#define WE_ISR_OVW	0x10		/* Overflow */
#define WE_ISR_CNT	0x20		/* Counter overflow */
#define WE_ISR_RST	0x80		/* reset */

#define WE_RSTAT_PRX	0x01		/* successful recv */
#define WE_RSTAT_CRC	0x02		/* CRC error */
#define WE_RSTAT_FAE	0x04		/* Frame alignment error */
#define WE_RSTAT_OVER	0x08		/* overflow */

char packet[1600];
int packetlen;
int bit16;

/**************************************************************************
ETH_PROBE - Look for an adapter
**************************************************************************/
eth_probe()
{
	unsigned short base;
	unsigned short chksum;
	unsigned char c;
	int i;
	for (we_base = WE_LOW_BASE; we_base <= WE_HIGH_BASE; we_base += 0x20) {
		chksum = 0;
		for (i=8; i<16; i++)
			chksum += inb(i+we_base);
		if ((chksum & 0x00FF) == 0x00FF)
			break;
	}
	if (we_base > WE_HIGH_BASE) return(0);	/* No adapter found */
	
	for (i = 1; i<6; i++)		/* Look for aliased registers */
		if (inb(we_base+i) != inb(we_base+i+WE_LAR)) break;
	if (i == 6) {		/* Aliased */
		we_bmem = (char *)WE_DEFAULT_MEM;
		bit16 = 0;
	} else {
		we_bmem = (char *)(0x80000 | ((inb(we_base+WE_MSR) & 0x3F) << 13));
		bit16 = 1;
	}
	outb(we_base+WE_MSR, 0x80);		/* Reset */
	printf("\r\nSMC80x3 base 0x%x, memory 0x%X, etheraddr ",we_base, we_bmem);
	for (i=0; i<6; i++)
		printhb((int)(arptable[ARP_CLIENT].node[i] = inb(i+we_base+WE_LAR)));
	printf("\r\n");
	outb(we_base+WE_MSR,(((unsigned)we_bmem >> 13) & 0x3F) | 0x40);
	iskey();		/* Kill some time while device resets */
	eth_reset();
}

/**************************************************************************
ETH_RESET - Reset adapter
**************************************************************************/
eth_reset()
{
	int i;
	outb(we_base+WE_P0_COMMAND, WE_COMMAND_PS0 | WE_COMMAND_STP);
	outb(we_base+WE_P0_DCR, 0x48);
	outb(we_base+WE_P0_RBCR0, 0);
	outb(we_base+WE_P0_RBCR1, 0);
	outb(we_base+WE_P0_RCR, 4);	/* allow broadcast frames */
	outb(we_base+WE_P0_TCR, 0);
	outb(we_base+WE_P0_TPSR, 0);
	outb(we_base+WE_P0_PSTART, WE_TXBUF_SIZE);
	outb(we_base+WE_P0_PSTOP, WE_RXBUF_END);	/* All cards have 8K */
	outb(we_base+WE_P0_BOUND, WE_TXBUF_SIZE);
	outb(we_base+WE_P0_IMR, 0);
	outb(we_base+WE_P0_ISR, 0xFF);
	if (bit16) outb(we_base+WE_LAAR, 1); 	/* Turn off 16bit mode */
	outb(we_base+WE_P0_COMMAND, WE_COMMAND_PS1);
	for (i=0; i<6; i++)
		outb(we_base+WE_P1_PAR0+i, inb(we_base+WE_LAR+i));
	for (i=0; i<6; i++)
		outb(we_base+WE_P1_MAR0+i, 0xFF);
	outb(we_base+WE_P1_CURR, WE_TXBUF_SIZE+1);
	outb(we_base+WE_P0_COMMAND, WE_COMMAND_PS0 | WE_COMMAND_STA);
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
	bcopy(d, we_bmem, 6);				/* Copy destination */
	bcopy(arptable[ARP_CLIENT].node, we_bmem+6, ETHER_ADDR_SIZE);	/* My ether addr */
	*(we_bmem+12) = t>>8;				/* Type field */
	*(we_bmem+13) = t;
	bcopy(p, we_bmem+14, s);
	s += 14;
	while (s < WE_MIN_PACKET) *(we_bmem+(s++)) = 0;
	twiddle();
	outb(we_base+WE_P0_COMMAND, WE_COMMAND_PS0);
	outb(we_base+WE_P0_TPSR, 0);
	outb(we_base+WE_P0_TBCR0, s);
	outb(we_base+WE_P0_TBCR1, s>>8);
	outb(we_base+WE_P0_COMMAND, WE_COMMAND_PS0 | WE_COMMAND_TXP);
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
	unsigned char *pkt, *p;
	rstat = inb(we_base+WE_P0_RSR);
	if (rstat & WE_RSTAT_OVER) {
		eth_reset();
		return(0);
	}
	if (!(rstat & WE_RSTAT_PRX)) return(0);
	bound = inb(we_base+WE_P0_BOUND)+1;
	if (bound == WE_RXBUF_END) bound = WE_TXBUF_SIZE;
	outb(we_base+WE_P0_COMMAND, WE_COMMAND_PS1);
	curr = inb(we_base+WE_P1_CURR);
	outb(we_base+WE_P0_COMMAND, WE_COMMAND_PS0);
	if (curr == WE_RXBUF_END) curr=WE_TXBUF_SIZE;
	if (curr == bound) return(0);
	pkt = we_bmem + (bound << 8);
	len = *((unsigned short *)(pkt+2)) - 4; /* sub CRC */
	if (len > 1514) len = 1514;
#ifdef DEBUG
printf("[R%dS%dC%dB%dN%d]",len, rstat, curr,bound,*(pkt+1));
#endif
	bound = *(pkt+1);		/* New bound ptr */
	p = packet;
	if ( (*pkt & WE_RSTAT_PRX) && (len > 14) && (len < 1518)) {
		pkt += 4;
		packetlen = len;
		while (len) {
			while (len && (pkt < (we_bmem + 8192))) {
				*(p++) = *(pkt++);
				len--;
			}
			pkt = we_bmem + (WE_TXBUF_SIZE << 8);
		}
		type = (packet[12]<<8) | packet[13];
		ret = 1;
	}
	if (bound == WE_TXBUF_SIZE)
		bound = WE_RXBUF_END;
	outb(we_base+WE_P0_BOUND, bound-1);
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

