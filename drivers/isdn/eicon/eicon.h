/* $Id: eicon.h,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN low-level module for Eicon active ISDN-Cards.
 *
 * Copyright 1998       by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1998-2000  by Armin Schindler (mac@melware.de) 
 * Copyright 1999,2000  Cytronics & Melware (info@melware.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef eicon_h
#define eicon_h

#define EICON_IOCTL_SETMMIO   0
#define EICON_IOCTL_GETMMIO   1
#define EICON_IOCTL_SETIRQ    2
#define EICON_IOCTL_GETIRQ    3
#define EICON_IOCTL_LOADBOOT  4
#define EICON_IOCTL_ADDCARD   5
#define EICON_IOCTL_GETTYPE   6
#define EICON_IOCTL_LOADPCI   7 
#define EICON_IOCTL_LOADISA   8 
#define EICON_IOCTL_GETVER    9 
#define EICON_IOCTL_GETXLOG  10 

#define EICON_IOCTL_MANIF    90 

#define EICON_IOCTL_FREEIT   97
#define EICON_IOCTL_TEST     98
#define EICON_IOCTL_DEBUGVAR 99

#define EICON_IOCTL_DIA_OFFSET	100

/* Bus types */
#define EICON_BUS_ISA          1
#define EICON_BUS_MCA          2
#define EICON_BUS_PCI          3

/* Constants for describing Card-Type */
#define EICON_CTYPE_S            0
#define EICON_CTYPE_SX           1
#define EICON_CTYPE_SCOM         2
#define EICON_CTYPE_QUADRO       3
#define EICON_CTYPE_S2M          4
#define EICON_CTYPE_MAESTRA      5
#define EICON_CTYPE_MAESTRAQ     6
#define EICON_CTYPE_MAESTRAQ_U   7
#define EICON_CTYPE_MAESTRAP     8
#define EICON_CTYPE_ISABRI       0x10
#define EICON_CTYPE_ISAPRI       0x20
#define EICON_CTYPE_MASK         0x0f
#define EICON_CTYPE_QUADRO_NR(n) (n<<4)

#define MAX_HEADER_LEN 10

#define MAX_STATUS_BUFFER	150

/* Struct for adding new cards */
typedef struct eicon_cdef {
        int membase;
        int irq;
        char id[10];
} eicon_cdef;

#define EICON_ISA_BOOT_MEMCHK 1
#define EICON_ISA_BOOT_NORMAL 2

/* Struct for downloading protocol via ioctl for ISA cards */
/* same struct for downloading protocol via ioctl for MCA cards */
typedef struct {
	/* start-up parameters */
	unsigned char tei;
	unsigned char nt2;
	unsigned char skip1;
	unsigned char WatchDog;
	unsigned char Permanent;
	unsigned char XInterface;
	unsigned char StableL2;
	unsigned char NoOrderCheck;
	unsigned char HandsetType;
	unsigned char skip2;
	unsigned char LowChannel;
	unsigned char ProtVersion;
	unsigned char Crc4;
	unsigned char Loopback;
	unsigned char oad[32];
	unsigned char osa[32];
	unsigned char spid[32];
	unsigned char boot_opt;
	unsigned long bootstrap_len;
	unsigned long firmware_len;
	unsigned char code[1]; /* Rest (bootstrap- and firmware code) will be allocated */
} eicon_isa_codebuf;

/* Data for downloading protocol via ioctl */
typedef union {
	eicon_isa_codebuf isa;
	eicon_isa_codebuf mca;
} eicon_codebuf;

/* Data for Management interface */
typedef struct {
	int count;
	int pos;
	int length[50];
	unsigned char data[700]; 
} eicon_manifbuf;

#define TRACE_OK                 (1)

#ifdef __KERNEL__

/* Kernel includes */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/ioport.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/pci.h>

#include <linux/isdn.h>
#include <linux/isdnif.h>


typedef struct {
  __u16 length __attribute__ ((packed)); /* length of data/parameter field */
  __u8  P[1];                          /* data/parameter field */
} eicon_PBUFFER;

#include "eicon_isa.h"

#include "idi.h"

typedef struct {
  __u16 NextReq  __attribute__ ((packed));  /* pointer to next Req Buffer */
  __u16 NextRc   __attribute__ ((packed));  /* pointer to next Rc Buffer  */
  __u16 NextInd  __attribute__ ((packed));  /* pointer to next Ind Buffer */
  __u8 ReqInput  __attribute__ ((packed));  /* number of Req Buffers sent */
  __u8 ReqOutput  __attribute__ ((packed)); /* number of Req Buffers returned */
  __u8 ReqReserved  __attribute__ ((packed));/*number of Req Buffers reserved */
  __u8 Int  __attribute__ ((packed));       /* ISDN-P interrupt           */
  __u8 XLock  __attribute__ ((packed));     /* Lock field for arbitration */
  __u8 RcOutput  __attribute__ ((packed));  /* number of Rc buffers received */
  __u8 IndOutput  __attribute__ ((packed)); /* number of Ind buffers received */
  __u8 IMask  __attribute__ ((packed));     /* Interrupt Mask Flag        */
  __u8 Reserved1[2]  __attribute__ ((packed)); /* reserved field, do not use */
  __u8 ReadyInt  __attribute__ ((packed));  /* request field for ready int */
  __u8 Reserved2[12]  __attribute__ ((packed)); /* reserved field, do not use */
  __u8 InterfaceType  __attribute__ ((packed)); /* interface type 1=16K    */
  __u16 Signature  __attribute__ ((packed));    /* ISDN-P initialized ind  */
  __u8 B[1];                            /* buffer space for Req,Ind and Rc */
} eicon_pr_ram;

/* Macro for delay via schedule() */
#define SLEEP(j) {                     \
  set_current_state(TASK_UNINTERRUPTIBLE); \
  schedule_timeout(j);                 \
}

typedef struct {
  __u8                  Req;            /* pending request          */
  __u8                  Rc;             /* return code received     */
  __u8                  Ind;            /* indication received      */
  __u8                  ReqCh;          /* channel of current Req   */
  __u8                  RcCh;           /* channel of current Rc    */
  __u8                  IndCh;          /* channel of current Ind   */
  __u8                  D3Id;           /* ID used by this entity   */
  __u8                  B2Id;           /* ID used by this entity   */
  __u8                  GlobalId;       /* reserved field           */
  __u8                  XNum;           /* number of X-buffers      */
  __u8                  RNum;           /* number of R-buffers      */
  struct sk_buff_head   X;              /* X-buffer queue           */
  struct sk_buff_head   R;              /* R-buffer queue           */
  __u8                  RNR;            /* receive not ready flag   */
  __u8                  complete;       /* receive complete status  */
  __u8                  busy;           /* busy flag                */
  __u16                 ref;            /* saved reference          */
} entity;

#define FAX_MAX_SCANLINE 256

typedef struct {
	__u8		PrevObject;
	__u8		NextObject;
	__u8		abLine[FAX_MAX_SCANLINE];
	__u8		abFrame[FAX_MAX_SCANLINE];
	unsigned int	LineLen;
	unsigned int	LineDataLen;
	__u32		LineData;
	unsigned int	NullBytesPos;
	__u8		NullByteExist;
	int		PageCount;
	__u8		Dle;
	__u8		Eop;
} eicon_ch_fax_buf;

typedef struct {
	int	       No;		 /* Channel Number	        */
	unsigned short fsm_state;        /* Current D-Channel state     */
	unsigned short statectrl;	 /* State controling bits	*/
	unsigned short eazmask;          /* EAZ-Mask for this Channel   */
	int		queued;          /* User-Data Bytes in TX queue */
	int		pqueued;         /* User-Data Packets in TX queue */
	int		waitq;           /* User-Data Bytes in wait queue */
	int		waitpq;          /* User-Data Bytes in packet queue */
	struct sk_buff *tskb1;           /* temp skb 1			*/
	struct sk_buff *tskb2;           /* temp skb 2			*/
	unsigned char  l2prot;           /* Layer 2 protocol            */
	unsigned char  l3prot;           /* Layer 3 protocol            */
#ifdef CONFIG_ISDN_TTY_FAX
	T30_s		*fax;		 /* pointer to fax data in LL	*/
	eicon_ch_fax_buf fax2;		 /* fax related struct		*/
#endif
	entity		e;		 /* Native Entity		*/
	ENTITY		de;		 /* Divas D Entity 		*/
	ENTITY		be;		 /* Divas B Entity 		*/
	char		cpn[32];	 /* remember cpn		*/
	char		oad[32];	 /* remember oad		*/
	char		dsa[32];	 /* remember dsa		*/
	char		osa[32];	 /* remember osa		*/
	unsigned char   cause[2];	 /* Last Cause			*/
	unsigned char	si1;
	unsigned char	si2;
	unsigned char	plan;
	unsigned char	screen;
} eicon_chan;

typedef struct {
	eicon_chan *ptr;
} eicon_chan_ptr;

#include "eicon_pci.h"

#define EICON_FLAGS_RUNNING  1 /* Cards driver activated */
#define EICON_FLAGS_PVALID   2 /* Cards port is valid    */
#define EICON_FLAGS_IVALID   4 /* Cards irq is valid     */
#define EICON_FLAGS_MVALID   8 /* Cards membase is valid */
#define EICON_FLAGS_LOADED   8 /* Firmware loaded        */

/* D-Channel states */
#define EICON_STATE_NULL     0
#define EICON_STATE_ICALL    1
#define EICON_STATE_OCALL    2
#define EICON_STATE_IWAIT    3
#define EICON_STATE_OWAIT    4
#define EICON_STATE_IBWAIT   5
#define EICON_STATE_OBWAIT   6
#define EICON_STATE_BWAIT    7
#define EICON_STATE_BHWAIT   8
#define EICON_STATE_BHWAIT2  9
#define EICON_STATE_DHWAIT  10
#define EICON_STATE_DHWAIT2 11
#define EICON_STATE_BSETUP  12
#define EICON_STATE_ACTIVE  13
#define EICON_STATE_ICALLW  14
#define EICON_STATE_LISTEN  15
#define EICON_STATE_WMCONN  16

#define EICON_MAX_QUEUE  2138

typedef union {
	eicon_isa_card isa;
	eicon_pci_card pci;
	eicon_isa_card mca;
} eicon_hwif;

typedef struct {
	__u8 ret;
	__u8 id;
	__u8 ch;
} eicon_ack;

typedef struct {
	__u8 code;
	__u8 id;
	__u8 ch;
} eicon_req;

typedef struct {
	__u8 ret;
	__u8 id;
	__u8 ch;
	__u8 more;
} eicon_indhdr;

/*
 * Per card driver data
 */
typedef struct eicon_card {
	eicon_hwif hwif;                 /* Hardware dependant interface     */
	DESCRIPTOR *d;			 /* IDI Descriptor		     */
        u_char ptype;                    /* Protocol type (1TR6 or Euro)     */
        u_char bus;                      /* Bustype (ISA, MCA, PCI)          */
        u_char type;                     /* Cardtype (EICON_CTYPE_...)       */
	struct eicon_card *qnext;  	 /* Pointer to next quadro adapter   */
        int Feature;                     /* Protocol Feature Value           */
        struct eicon_card *next;	 /* Pointer to next device struct    */
        int myid;                        /* Driver-Nr. assigned by linklevel */
        unsigned long flags;             /* Statusflags                      */
	struct sk_buff_head rcvq;        /* Receive-Message queue            */
	struct sk_buff_head sndq;        /* Send-Message queue               */
	struct sk_buff_head rackq;       /* Req-Ack-Message queue            */
	struct sk_buff_head sackq;       /* Data-Ack-Message queue           */
	struct sk_buff_head statq;       /* Status-Message queue             */
	int statq_entries;
	struct tq_struct snd_tq;         /* Task struct for xmit bh          */
	struct tq_struct rcv_tq;         /* Task struct for rcv bh           */
	struct tq_struct ack_tq;         /* Task struct for ack bh           */
	eicon_chan*	IdTable[256];	 /* Table to find entity   */
	__u16  ref_in;
	__u16  ref_out;
	int    nchannels;                /* Number of B-Channels             */
	int    ReadyInt;		 /* Ready Interrupt		     */
	eicon_chan *bch;                 /* B-Channel status/control         */
	DBUFFER *dbuf;			 /* Dbuffer for Diva Server	     */
	BUFFERS *sbuf;			 /* Buffer for Diva Server	     */
	char *sbufp;			 /* Data Buffer for Diva Server	     */
        isdn_if interface;               /* Interface to upper layer         */
        char regname[35];                /* Drivers card name 		     */
#ifdef CONFIG_MCA
        int	mca_slot;	 	 /* # of cards MCA slot              */
	int	mca_io;			 /* MCA cards IO port		     */
#endif /* CONFIG_MCA */
} eicon_card;

#include "eicon_idi.h"

extern eicon_card *cards;
extern char *eicon_ctype_name[];


static inline void eicon_schedule_tx(eicon_card *card)
{
        queue_task(&card->snd_tq, &tq_immediate);
        mark_bh(IMMEDIATE_BH);
}

static inline void eicon_schedule_rx(eicon_card *card)
{
        queue_task(&card->rcv_tq, &tq_immediate);
        mark_bh(IMMEDIATE_BH);
}

static inline void eicon_schedule_ack(eicon_card *card)
{
        queue_task(&card->ack_tq, &tq_immediate);
        mark_bh(IMMEDIATE_BH);
}

extern int eicon_addcard(int, int, int, char *, int);
extern void eicon_io_transmit(eicon_card *card);
extern void eicon_irq(int irq, void *dev_id, struct pt_regs *regs);
extern void eicon_io_rcv_dispatch(eicon_card *ccard);
extern void eicon_io_ack_dispatch(eicon_card *ccard);
#ifdef CONFIG_MCA
extern int eicon_mca_find_card(int, int, int, char *);
extern int eicon_mca_probe(int, int, int, int, char *);
extern int eicon_info(char *, int , void *);
#endif /* CONFIG_MCA */

extern ulong DebugVar;
extern void eicon_log(eicon_card * card, int level, const char *fmt, ...);
extern void eicon_putstatus(eicon_card * card, char * buf);

extern spinlock_t eicon_lock;

#endif  /* __KERNEL__ */

#endif	/* eicon_h */
