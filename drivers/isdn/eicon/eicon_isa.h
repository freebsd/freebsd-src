/* $Id: eicon_isa.h,v 1.1.4.1 2001/11/20 14:19:35 kai Exp $
 *
 * ISDN low-level module for Eicon active ISDN-Cards.
 *
 * Copyright 1998      by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1998-2000 by Armin Schindler (mac@melware.de)
 * Copyright 1999,2000 Cytronics & Melware (info@melware.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef eicon_isa_h
#define eicon_isa_h

#ifdef __KERNEL__
#include <linux/config.h>

/* Factory defaults for ISA-Cards */
#define EICON_ISA_MEMBASE 0xd0000
#define EICON_ISA_IRQ     3
/* shmem offset for Quadro parts */
#define EICON_ISA_QOFFSET 0x0800

typedef struct {
        __u16 length __attribute__ ((packed));   /* length of data/parameter field         */
        __u8  P[270];                            /* data/parameter field                   */
} eicon_scom_PBUFFER;

/* General communication buffer */
typedef struct {
        __u8   Req;                                /* request register                       */
	__u8   ReqId;                              /* request task/entity identification     */
	__u8   Rc;                                 /* return code register                   */
	__u8   RcId;                               /* return code task/entity identification */
	__u8   Ind;                                /* Indication register                    */
	__u8   IndId;                              /* Indication task/entity identification  */
	__u8   IMask;                              /* Interrupt Mask Flag                    */
	__u8   RNR;                                /* Receiver Not Ready (set by PC)         */
	__u8   XLock;                              /* XBuffer locked Flag                    */
	__u8   Int;                                /* ISDN interrupt                         */
	__u8   ReqCh;                              /* Channel field for layer-3 Requests     */
	__u8   RcCh;                               /* Channel field for layer-3 Returncodes  */
	__u8   IndCh;                              /* Channel field for layer-3 Indications  */
	__u8   MInd;                               /* more data indication field             */
	__u16  MLength;                            /* more data total packet length          */
	__u8   ReadyInt;                           /* request field for ready interrupt      */
	__u8   Reserved[12];                       /* reserved space                         */
	__u8   IfType;                             /* 1 = 16k-Interface                      */
	__u16  Signature __attribute__ ((packed)); /* ISDN adapter Signature                 */
	eicon_scom_PBUFFER XBuffer;                /* Transmit Buffer                        */
	eicon_scom_PBUFFER RBuffer;                /* Receive Buffer                         */
} eicon_isa_com;

/* struct for downloading firmware */
typedef struct {
	__u8  ctrl;
	__u8  card;
	__u8  msize;
	__u8  fill0;
	__u16 ebit __attribute__ ((packed));
	__u32 eloc __attribute__ ((packed));
	__u8  reserved[20];
	__u16 signature __attribute__ ((packed));
	__u8  fill[224];
	__u8  b[256];
} eicon_isa_boot;

/* Shared memory */
typedef union {
	unsigned char  c[0x400];
	eicon_isa_com  com;
	eicon_isa_boot boot;
} eicon_isa_shmem;

/*
 * card's description
 */
typedef struct {
	int               ramsize;
	int               irq;	    /* IRQ                        */
	unsigned long	  physmem;  /* physical memory address	  */
#ifdef CONFIG_MCA
	int		  io;	    /* IO-port for MCA brand      */
#endif /* CONFIG_MCA */
	void*             card;
	eicon_isa_shmem*  shmem;    /* Shared-memory area         */
	unsigned char*    intack;   /* Int-Acknowledge            */
	unsigned char*    stopcpu;  /* Writing here stops CPU     */
	unsigned char*    startcpu; /* Writing here starts CPU    */
	unsigned char     type;     /* card type                  */
	int		  channels; /* No. of channels		  */
	unsigned char     irqprobe; /* Flag: IRQ-probing          */
	unsigned char     mvalid;   /* Flag: Memory is valid      */
	unsigned char     ivalid;   /* Flag: IRQ is valid         */
	unsigned char     master;   /* Flag: Card ist Quadro 1/4  */
} eicon_isa_card;

/* Offsets for special locations on standard cards */
#define INTACK     0x03fe 
#define STOPCPU    0x0400
#define STARTCPU   0x0401
#define RAMSIZE    0x0400
/* Offsets for special location on PRI card */
#define INTACK_P   0x3ffc
#define STOPCPU_P  0x3ffe
#define STARTCPU_P 0x3fff
#define RAMSIZE_P  0x4000


extern int eicon_isa_load(eicon_isa_card *card, eicon_isa_codebuf *cb);
extern int eicon_isa_bootload(eicon_isa_card *card, eicon_isa_codebuf *cb);
extern void eicon_isa_release(eicon_isa_card *card);
extern void eicon_isa_printpar(eicon_isa_card *card);
extern void eicon_isa_transmit(eicon_isa_card *card);
extern int eicon_isa_find_card(int Mem, int Irq, char * Id);

#endif  /* __KERNEL__ */

#endif	/* eicon_isa_h */
