#define FEPCODESEG  0x0200L
#define FEPCODE     0x2000L
#define BIOSCODE    0xf800L

#define MISCGLOBAL  0x0C00L
#define NPORT       0x0C22L
#define MBOX        0x0C40L
#define PORTBASE    0x0C90L

#define FEPCLR      0x00
#define FEPMEM      0x02
#define FEPRST      0x04
#define FEPINT      0x08
#define	FEPMASK     0x0e
#define	FEPWIN      0x80

/* Maximum Number of Boards supported */
#define MAX_DIGI_BOARDS 4

#define PCXX_NUM_TYPES	4

#define PCXI		0
#define PCXE		1
#define	PCXEVE		2
#define PCXEM		3

static char *board_desc[] = {
	"PC/Xi",
	"PC/Xe",
	"PC/Xeve",
	"PC/Xem",
};

static char *board_mem[] = {
	"64k",
	"64k",
	"8k",
	"32k",
};
#define STARTC      021
#define STOPC       023
#define IAIXON      0x2000


struct board_info	{
	unchar status;
	unchar type;
	unchar altpin;
	ushort numports;
	ushort port;
	ulong  membase;
	ulong  memsize;
	ushort first_minor;
	void *region;
};


#define TXSTOPPED   0x01
#define LOWWAIT		0x02
#define EMPTYWAIT	0x04
#define RXSTOPPED	0x08
#define TXBUSY		0x10

#define DISABLED   0
#define ENABLED    1
#define OFF        0
#define ON         1

#define FEPTIMEOUT 200000  
#define SERIAL_TYPE_NORMAL	1
#define SERIAL_TYPE_CALLOUT	2
#define PCXE_EVENT_HANGUP   1
#define PCXX_MAGIC	0x5c6df104L

struct channel {
							/* --------- Board/channel information ---------- */
	long						magic;
	unchar						boardnum;
	unchar						channelnum;
	uint						dev;
	long						session;
	long						pgrp;
	struct tty_struct			*tty;
	struct board_info			*board;
	volatile struct board_chan	*brdchan;
	volatile struct global_data *mailbox;
	int							asyncflags;
	int							count;
	int							blocked_open;
	int							close_delay;
	int							event;
	wait_queue_head_t			open_wait;
	wait_queue_head_t			close_wait;
	struct tq_struct			tqueue;
							/* ------------ Async control data ------------- */
	unchar						modemfake;      /* Modem values to be forced */
	unchar						modem;          /* Force values */
	ulong						statusflags;
	unchar						omodem;         /* FEP output modem status */
	unchar						imodem;         /* FEP input modem status */
	unchar						hflow;
	unchar						dsr;
	unchar						dcd;
	unchar						stopc;
	unchar						startc;
	unchar						stopca;
	unchar						startca;
	unchar						fepstopc;
	unchar						fepstartc;
	unchar						fepstopca;
	unchar						fepstartca;
	ushort						fepiflag;
	ushort						fepcflag;
	ushort						fepoflag;
							/* ---------- Transmit/receive system ---------- */
	unchar						txwin;
	unchar						rxwin;
	ushort						txbufsize;
	ushort						rxbufsize;
	unchar						*txptr;
	unchar						*rxptr;
	unchar						*tmp_buf;		/* Temp buffer */
	struct semaphore				tmp_buf_sem;
							/* ---- Termios data ---- */
	ulong						c_iflag;
	ulong						c_cflag;
	ulong						c_lflag;
	ulong						c_oflag;
	struct termios				normal_termios;
	struct termios				callout_termios;
	struct digi_struct			digiext;
	ulong						dummy[8];
};
