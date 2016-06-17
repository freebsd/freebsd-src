#ifndef _LINUX_X25_ASY_H
#define _LINUX_X25_ASY_H

/* X.25 asy configuration. */
#define SL_NRUNIT	256		/* MAX number of X.25 channels;
					   This can be overridden with
					   insmod -ox25_asy_maxdev=nnn	*/
#define SL_MTU		256	

/* X25 async protocol characters. */
#define X25_END         0x7E		/* indicates end of frame	*/
#define X25_ESC         0x7D		/* indicates byte stuffing	*/
#define X25_ESCAPE(x)	((x)^0x20)
#define X25_UNESCAPE(x)	((x)^0x20)


struct x25_asy {
  int			magic;

  /* Various fields. */
  struct tty_struct	*tty;		/* ptr to TTY structure		*/
  struct net_device		*dev;		/* easy for intr handling	*/

  /* These are pointers to the malloc()ed frame buffers. */
  unsigned char		*rbuff;		/* receiver buffer		*/
  int                   rcount;         /* received chars counter       */
  unsigned char		*xbuff;		/* transmitter buffer		*/
  unsigned char         *xhead;         /* pointer to next byte to XMIT */
  int                   xleft;          /* bytes left in XMIT queue     */

  /* X.25 interface statistics. */
  unsigned long		rx_packets;	/* inbound frames counter	*/
  unsigned long         tx_packets;     /* outbound frames counter      */
  unsigned long		rx_bytes;	/* inbound byte counte		*/
  unsigned long         tx_bytes;       /* outbound byte counter	*/
  unsigned long         rx_errors;      /* Parity, etc. errors          */
  unsigned long         tx_errors;      /* Planned stuff                */
  unsigned long         rx_dropped;     /* No memory for skb            */
  unsigned long         tx_dropped;     /* When MTU change              */
  unsigned long         rx_over_errors; /* Frame bigger then X.25 buf.  */

  int			mtu;		/* Our mtu (to spot changes!)   */
  int                   buffsize;       /* Max buffers sizes            */

  unsigned long		flags;		/* Flag values/ mode etc	*/
#define SLF_INUSE	0		/* Channel in use               */
#define SLF_ESCAPE	1               /* ESC received                 */
#define SLF_ERROR	2               /* Parity, etc. error           */
#define SLF_OUTWAIT	4		/* Waiting for output		*/
};



#define X25_ASY_MAGIC 0x5303

extern int x25_asy_init(struct net_device *dev);

#endif	/* _LINUX_X25_ASY.H */
