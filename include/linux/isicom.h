#ifndef _LINUX_ISICOM_H
#define _LINUX_ISICOM_H

/*#define		ISICOM_DEBUG*/
/*#define		ISICOM_DEBUG_DTR_RTS*/


/*
 *	Firmware Loader definitions ...
 */
 
#define		__MultiTech		('M'<<8)
#define		MIOCTL_LOAD_FIRMWARE	(__MultiTech | 0x01)
#define         MIOCTL_READ_FIRMWARE    (__MultiTech | 0x02)
#define         MIOCTL_XFER_CTRL	(__MultiTech | 0x03)
#define         MIOCTL_RESET_CARD	(__MultiTech | 0x04)

#define		DATA_SIZE	16

typedef	struct	{
		unsigned short	exec_segment;
		unsigned short	exec_addr;
}	exec_record;

typedef	struct	{
		int		board;		/* Board to load */
		unsigned short	addr;
		unsigned short	count;
}	bin_header;

typedef	struct	{
		int		board;		/* Board to load */
		unsigned short	addr;
		unsigned short	count;
		unsigned short	segment;
		unsigned char	bin_data[DATA_SIZE];
}	bin_frame;

#ifdef __KERNEL__

#define		YES	1
#define		NO	0

#define		ISILOAD_MISC_MINOR	155	/* /dev/isctl */
#define		ISILOAD_NAME		"ISILoad"

/*	
 *  ISICOM Driver definitions ...
 *
 */

#define		ISICOM_NAME	"ISICom"

/*
 *      PCI definitions
 */

 #define        DEVID_COUNT     9
 #define        VENDOR_ID       0x10b5

/*
 *	These are now officially allocated numbers
 */

#define		ISICOM_NMAJOR	112	/* normal  */
#define		ISICOM_CMAJOR	113	/* callout */
#define		ISICOM_MAGIC	(('M' << 8) | 'T')

#define		WAKEUP_CHARS	256	/* hard coded for now	*/ 
#define		TX_SIZE		254 
 
#define		BOARD_COUNT	4
#define		PORT_COUNT	(BOARD_COUNT*16)

#define		SERIAL_TYPE_NORMAL	1
#define		SERIAL_TYPE_CALLOUT	2

/*   character sizes  */

#define		ISICOM_CS5		0x0000
#define		ISICOM_CS6		0x0001
#define		ISICOM_CS7		0x0002
#define		ISICOM_CS8		0x0003

/* stop bits */

#define		ISICOM_1SB		0x0000
#define		ISICOM_2SB		0x0004

/* parity */

#define		ISICOM_NOPAR		0x0000
#define		ISICOM_ODPAR		0x0008
#define		ISICOM_EVPAR		0x0018

/* flow control */

#define		ISICOM_CTSRTS		0x03
#define		ISICOM_INITIATE_XONXOFF	0x04
#define		ISICOM_RESPOND_XONXOFF	0x08

#define InterruptTheCard(base) (outw(0,(base)+0xc)) 
#define ClearInterrupt(base) (inw((base)+0x0a))	

#define	BOARD(line)  (((line) >> 4) & 0x3)
#define MIN(a, b) ( (a) < (b) ? (a) : (b) )

	/*	isi kill queue bitmap	*/
	
#define		ISICOM_KILLTX		0x01
#define		ISICOM_KILLRX		0x02

	/* isi_board status bitmap */
	
#define		FIRMWARE_LOADED		0x0001
#define		BOARD_ACTIVE		0x0002

 	/* isi_port status bitmap  */

#define		ISI_CTS			0x1000
#define		ISI_DSR			0x2000
#define		ISI_RI			0x4000
#define		ISI_DCD			0x8000
#define		ISI_DTR			0x0100
#define		ISI_RTS			0x0200


#define		ISI_TXOK		0x0001 
 
struct	isi_board {
	unsigned short		base;
	unsigned char		irq;
	unsigned char		port_count;
	unsigned short		status;
	unsigned short		port_status; /* each bit represents a single port */
	unsigned short		shift_count;
	struct isi_port		* ports;
	signed char		count;
	unsigned char		isa;
};

struct	isi_port {
	unsigned short		magic;
	unsigned int		flags;
	int			count;
	int			blocked_open;
	int			close_delay;
	unsigned short		channel;
	unsigned short		status;
	unsigned short		closing_wait;
	long 			session;
	long			pgrp;
	struct isi_board	* card;
	struct tty_struct 	* tty;
	wait_queue_head_t	close_wait;
	wait_queue_head_t	open_wait;
	struct tq_struct	hangup_tq;
	struct tq_struct	bh_tqueue;
	unsigned char		* xmit_buf;
	int			xmit_head;
	int			xmit_tail;
	int			xmit_cnt;
	struct termios 		normal_termios;
	struct termios		callout_termios;
};


/*
 *  ISI Card specific ops ...
 */
 
static inline void raise_dtr(struct isi_port * port)
{
	struct isi_board * card = port->card;
	unsigned short base = card->base;
	unsigned char channel = port->channel;
	short wait=400;
	while(((inw(base+0x0e) & 0x01) == 0) && (wait-- > 0));
	if (wait <= 0) {
		printk(KERN_WARNING "ISICOM: Card found busy in raise_dtr.\n");
		return;
	}
#ifdef ISICOM_DEBUG_DTR_RTS	
	printk(KERN_DEBUG "ISICOM: raise_dtr.\n");
#endif	
	outw(0x8000 | (channel << card->shift_count) | 0x02 , base);
	outw(0x0504, base);
	InterruptTheCard(base);
	port->status |= ISI_DTR;
}

static inline void drop_dtr(struct isi_port * port)
{	
	struct isi_board * card = port->card;
	unsigned short base = card->base;
	unsigned char channel = port->channel;
	short wait=400;
	while(((inw(base+0x0e) & 0x01) == 0) && (wait-- > 0));
	if (wait <= 0) {
		printk(KERN_WARNING "ISICOM: Card found busy in drop_dtr.\n");
		return;
	}
#ifdef ISICOM_DEBUG_DTR_RTS	
	printk(KERN_DEBUG "ISICOM: drop_dtr.\n");
#endif	
	outw(0x8000 | (channel << card->shift_count) | 0x02 , base);
	outw(0x0404, base);
	InterruptTheCard(base);	
	port->status &= ~ISI_DTR;
}
static inline void raise_rts(struct isi_port * port)
{
	struct isi_board * card = port->card;
	unsigned short base = card->base;
	unsigned char channel = port->channel;
	short wait=400;
	while(((inw(base+0x0e) & 0x01) == 0) && (wait-- > 0));
	if (wait <= 0) {
		printk(KERN_WARNING "ISICOM: Card found busy in raise_rts.\n");
		return;
	}
#ifdef ISICOM_DEBUG_DTR_RTS	
	printk(KERN_DEBUG "ISICOM: raise_rts.\n");
#endif	
	outw(0x8000 | (channel << card->shift_count) | 0x02 , base);
	outw(0x0a04, base);
	InterruptTheCard(base);	
	port->status |= ISI_RTS;
}
static inline void drop_rts(struct isi_port * port)
{
	struct isi_board * card = port->card;
	unsigned short base = card->base;
	unsigned char channel = port->channel;
	short wait=400;
	while(((inw(base+0x0e) & 0x01) == 0) && (wait-- > 0));
	if (wait <= 0) {
		printk(KERN_WARNING "ISICOM: Card found busy in drop_rts.\n");
		return;
	}
#ifdef ISICOM_DEBUG_DTR_RTS	
	printk(KERN_DEBUG "ISICOM: drop_rts.\n");
#endif	
	outw(0x8000 | (channel << card->shift_count) | 0x02 , base);
	outw(0x0804, base);
	InterruptTheCard(base);	
	port->status &= ~ISI_RTS;
}
static inline void raise_dtr_rts(struct isi_port * port)
{
	struct isi_board * card = port->card;
	unsigned short base = card->base;
	unsigned char channel = port->channel;
	short wait=400;
	while(((inw(base+0x0e) & 0x01) == 0) && (wait-- > 0));
	if (wait <= 0) {
		printk(KERN_WARNING "ISICOM: Card found busy in raise_dtr_rts.\n");
		return;
	}
#ifdef ISICOM_DEBUG_DTR_RTS	
	printk(KERN_DEBUG "ISICOM: raise_dtr_rts.\n");
#endif	
	outw(0x8000 | (channel << card->shift_count) | 0x02 , base);
	outw(0x0f04, base);
	InterruptTheCard(base);
	port->status |= (ISI_DTR | ISI_RTS);
}
static inline void drop_dtr_rts(struct isi_port * port)
{
	struct isi_board * card = port->card;
	unsigned short base = card->base;
	unsigned char channel = port->channel;
	short wait=400;
	while(((inw(base+0x0e) & 0x01) == 0) && (wait-- > 0));
	if (wait <= 0) {
		printk(KERN_WARNING "ISICOM: Card found busy in drop_dtr_rts.\n");
		return;
	}
#ifdef ISICOM_DEBUG_DTR_RTS	
	printk(KERN_DEBUG "ISICOM: drop_dtr_rts.\n");
#endif	
	outw(0x8000 | (channel << card->shift_count) | 0x02 , base);
	outw(0x0c04, base);
	InterruptTheCard(base);	
	port->status &= ~(ISI_RTS | ISI_DTR);
}

static inline void kill_queue(struct isi_port * port, short queue)
{
	struct isi_board * card = port->card;
	unsigned short base = card->base;
	unsigned char channel = port->channel;
	short wait=400;
	while(((inw(base+0x0e) & 0x01) == 0) && (wait-- > 0));
	if (wait <= 0) {
		printk(KERN_WARNING "ISICOM: Card found busy in kill_queue.\n");
		return;
	}
#ifdef ISICOM_DEBUG	
	printk(KERN_DEBUG "ISICOM: kill_queue 0x%x.\n", queue);
#endif	
	outw(0x8000 | (channel << card->shift_count) | 0x02 , base);
	outw((queue << 8) | 0x06, base);
	InterruptTheCard(base);	
}

#endif	/*	__KERNEL__	*/

#endif	/*	ISICOM_H	*/

