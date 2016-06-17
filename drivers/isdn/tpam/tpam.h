/* $Id: tpam.h,v 1.1.2.1 2001/11/20 14:19:37 kai Exp $
 *
 * Turbo PAM ISDN driver for Linux. (Kernel Driver)
 *
 * Copyright 2001 Stelian Pop <stelian.pop@fr.alcove.com>, Alcôve
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * For all support questions please contact: <support@auvertech.fr>
 *
 */

#ifndef _TPAM_PRIV_H_
#define _TPAM_PRIV_H_

#include <linux/isdnif.h>
#include <linux/init.h>

/* Maximum number of channels for this board */
#define TPAM_NBCHANNEL		30
/* Maximum size of data */
#define TPAM_MAXBUFSIZE		2032
/* Size of a page of board memory */
#define TPAM_PAGE_SIZE		0x003ffffc	/* 4 MB */
/* Magic number present if the board was successfully started */
#define TPAM_MAGICNUMBER	0x2a343242

/* Registers in the PCI BAR0 */
#define TPAM_PAGE_REGISTER	0x00400000	/* Select page */
#define TPAM_DSPINT_REGISTER	0x00400004	/* Interrupt board */
#define TPAM_RESETPAM_REGISTER	0x00400008	/* Reset board */
#define TPAM_HINTACK_REGISTER	0x0040000c	/* Ack interrupt */
#define TPAM_HPIC_REGISTER	0x00400014	/* Board ready */

/* Registers in the board memory */
#define TPAM_MAGICNUMBER_REGISTER	0x80008000 /* Magic number */
#define TPAM_EXID_REGISTER		0x80008004 /* EXID - not used */
#define TPAM_UPLOADPTR_REGISTER		0x80008008 /* Upload data ptr */
#define TPAM_DOWNLOADPTR_REGISTER	0x8000800c /* Download data ptr */
#define TPAM_ACKUPLOAD_REGISTER		0x80008010 /* Ack upload */
#define TPAM_ACKDOWNLOAD_REGISTER	0x80008014 /* Ack download */
#define TPAM_INTERRUPTACK_REGISTER	0x80008018 /* Ack interrupt */

/* Reserved areas in the board memory */
#define	TPAM_RESERVEDAREA1_START	0x00000000
#define TPAM_RESERVEDAREA1_END		0x003FFFFF
#define TPAM_RESERVEDAREA2_START	0x01C00000
#define TPAM_RESERVEDAREA2_END		0x01FFFFFF
#define TPAM_RESERVEDAREA3_START	0x04000000
#define TPAM_RESERVEDAREA3_END		0x7FFFFFFF
#define TPAM_RESERVEDAREA4_START	0x80010000
#define TPAM_RESERVEDAREA4_END		0xFFFFFFFF

/* NCO ID invalid */
#define TPAM_NCOID_INVALID	0xffff
/* channel number invalid */
#define TPAM_CHANNEL_INVALID	0xffff

/* Channel structure */
typedef struct tpam_channel {
	int num;			/* channel number */
	struct tpam_card *card;		/* channel's card */
	u32 ncoid;			/* ncoid */
	u8  hdlc;			/* hdlc mode (set by user level) */
	u8  realhdlc;			/* hdlc mode (negociated with peer) */
	u32 hdlcshift;			/* hdlc shift */
	u8  readytoreceive;		/* channel ready to receive data */
	struct sk_buff_head sendq;	/* Send queue */
} tpam_channel;

/* Card structure */
typedef struct tpam_card {
	struct tpam_card *next;		/* next card in list */
	unsigned int irq;		/* IRQ used by this board */
	unsigned long bar0;		/* ioremapped bar0 */
	int id;				/* id of the board */
	isdn_if interface;		/* isdn link-level pointer */
	int channels_used;		/* number of channels actually used */
	int channels_tested;		/* number of channels being tested */
	u8 loopmode;			/* board in looptest mode */
	tpam_channel channels[TPAM_NBCHANNEL];/* channels tab */
	int running;			/* card is running */
	int busy;			/* waiting for ack from card */
	int roundrobin;			/* round robin between channels */
	struct sk_buff_head sendq;	/* send queue */
	struct sk_buff_head recvq;	/* receive queue */
	struct tq_struct send_tq;	/* send task queue */
	struct tq_struct recv_tq;	/* receive task queue */
	spinlock_t lock;		/* lock for the card */
} tpam_card;

/* Timeout waiting for signature to become available */
#define SIGNATURE_TIMEOUT	(5*HZ)
/* Timeout waiting for receiving all the ACreateNCOCnf */
#define NCOCREATE_TIMEOUT	(30*HZ)

/* Maximum size of the TLV block */
#define MPB_MAXIMUMBLOCKTLVSIZE	 128
/* Maximum size of the data block */
#define MPB_MAXIMUMDATASIZE	4904
/* Maximum size of a phone number */
#define PHONE_MAXIMUMSIZE	  32

/* Header for a sk_buff structure */
typedef struct skb_header {
	u16 size;		/* size of pci_mpb block + size of tlv block */
	u16 data_size;		/* size of the data block */
	u16 ack;		/* packet needs to send ack upon send */
	u16 ack_size;		/* size of data to be acknowledged upon send */
} skb_header;

/* PCI message header structure */
typedef struct pci_mpb {
	u16 exID;			/* exID - not used */
	u16 flags;			/* flags - not used */
	u32 errorCode;			/* errorCode - not used */
	u16 messageID;			/* message ID - one of ID_XXX */
	u16 maximumBlockTLVSize;	/* MPB_MAXIMUMBLOCKTLVSIZE */
	u16 actualBlockTLVSize;		/* size of the tlv block */
	u16 maximumDataSize;		/* MPB_MAXIMUMDATASIZE */
	u16 actualDataSize;		/* size of the data block */
	u16 dummy;			/* padding */
} pci_mpb;

/* Types of PCI messages */
#define ID_ACreateNCOReq	101
#define ID_ACreateNCOCnf	102
#define ID_ADestroyNCOReq	103
#define ID_ADestroyNCOCnf	104
#define ID_CConnectReq		203
#define ID_CConnectInd		204
#define ID_CConnectRsp		205
#define ID_CConnectCnf		206
#define ID_CDisconnectReq	207
#define ID_CDisconnectInd	208
#define ID_CDisconnectRsp	209
#define ID_CDisconnectCnf	210
#define ID_U3DataReq		307
#define ID_U3DataInd		308
#define ID_U3ReadyToReceiveInd	318

/* Parameters for the PCI message TLV block */
#define PAR_BearerCap		3
#define PAR_CalledNumber	7
#define PAR_CallingNumber	11
#define PAR_CauseToPUF		15
#define PAR_Cdirection		16
#define PAR_CompletionStatus	19
#define PAR_Facility		30
#define PAR_HLC			34
#define PAR_NCOID		49
#define PAR_NCOType		50
#define PAR_ReadyFlag		55
#define PAR_U3Protocol		62
#define PAR_Udirection		64

/* Delayed statcallb structure */
typedef struct tpam_statcallb_data {
	tpam_card *card;		/* card issuing the statcallb */
	struct timer_list *timer;	/* timer launching the statcallb */
	isdn_ctrl ctrl;			/* isdn command */
} tpam_statcallb_data;

/* Function prototypes from tpam_main.c */
extern tpam_card *tpam_findcard(int);
extern u32 tpam_findchannel(tpam_card *, u32);

/* Function prototypes from tpam_memory.c */
extern void copy_to_pam_dword(tpam_card *, const void *, u32);
extern void copy_to_pam(tpam_card *, void *, const void *, u32);
extern u32 copy_from_pam_dword(tpam_card *, const void *);
extern void copy_from_pam(tpam_card *, void *, const void *, u32);
extern int copy_from_pam_to_user(tpam_card *, void *, const void *, u32);
extern int copy_from_user_to_pam(tpam_card *, void *, const void *, u32);
extern int tpam_verify_area(u32, u32);

/* Function prototypes from tpam_nco.c */
extern struct sk_buff *build_ACreateNCOReq(const u8 *);
extern struct sk_buff *build_ADestroyNCOReq(u32);
extern struct sk_buff *build_CConnectReq(u32, const u8 *, u8);
extern struct sk_buff *build_CConnectRsp(u32);
extern struct sk_buff *build_CDisconnectReq(u32);
extern struct sk_buff *build_CDisconnectRsp(u32);
extern struct sk_buff *build_U3DataReq(u32, void *, u16, u16, u16);
extern int parse_ACreateNCOCnf(struct sk_buff *, u8 *, u32 *);
extern int parse_ADestroyNCOCnf(struct sk_buff *, u8 *, u32 *);
extern int parse_CConnectCnf(struct sk_buff *, u32 *);
extern int parse_CConnectInd(struct sk_buff *, u32 *, u8 *, u8 *, 
			     u8 *, u8 *, u8 *);
extern int parse_CDisconnectCnf(struct sk_buff *, u32 *, u32 *);
extern int parse_CDisconnectInd(struct sk_buff *, u32 *, u32 *);
extern int parse_U3ReadyToReceiveInd(struct sk_buff *, u32 *, u8 *);
extern int parse_U3DataInd(struct sk_buff *, u32 *, u8 **, u16 *);

/* Function prototypes from tpam_queues.c */
extern void tpam_enqueue(tpam_card *, struct sk_buff *);
extern void tpam_enqueue_data(tpam_channel *, struct sk_buff *);
extern void tpam_irq(int, void *, struct pt_regs *);
extern void tpam_recv_tq(tpam_card *);
extern void tpam_send_tq(tpam_card *);

/* Function prototypes from tpam_commands.c */
extern int tpam_command(isdn_ctrl *);
extern int tpam_writebuf_skb(int, int, int, struct sk_buff *);
extern void tpam_recv_ACreateNCOCnf(tpam_card *, struct sk_buff *);
extern void tpam_recv_ADestroyNCOCnf(tpam_card *, struct sk_buff *);
extern void tpam_recv_CConnectCnf(tpam_card *, struct sk_buff *);
extern void tpam_recv_CConnectInd(tpam_card *, struct sk_buff *);
extern void tpam_recv_CDisconnectInd(tpam_card *, struct sk_buff *);
extern void tpam_recv_CDisconnectCnf(tpam_card *, struct sk_buff *);
extern void tpam_recv_U3DataInd(tpam_card *, struct sk_buff *);
extern void tpam_recv_U3ReadyToReceiveInd(tpam_card *, struct sk_buff *);

/* Function prototypes from tpam_hdlc.c */
extern u32 tpam_hdlc_encode(u8 *, u8 *, u32 *, u32);
extern u32 tpam_hdlc_decode(u8 *, u8 *, u32);

/* Function prototypes from tpam_crcpc.c */
extern void init_CRC(void);
extern void hdlc_encode_modem(u8 *, u32, u8 *, u32 *);
extern void hdlc_no_accm_encode(u8 *, u32, u8 *, u32 *);
extern u32 hdlc_no_accm_decode(u8 *, u32);

/* Define this to enable debug tracing prints */
#undef DEBUG

#ifdef DEBUG
#define dprintk printk
#else
#define dprintk while(0) printk
#endif

#endif /* _TPAM_H_ */
