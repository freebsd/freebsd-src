/*********************************************************************
 *                
 * Filename:      toshoboe.h
 * Version:       0.1
 * Description:   Driver for the Toshiba OBOE (or type-O)
 *                FIR Chipset. 
 * Status:        Experimental.
 * Author:        James McKenzie <james@fishsoup.dhs.org>
 * Created at:    Sat May 8  12:35:27 1999
 * 
 *     Copyright (c) 1999-2000 James McKenzie, All Rights Reserved.
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither James McKenzie nor Cambridge University admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *
 *     Applicable Models : Libretto 100CT. and many more
 *
 ********************************************************************/

#ifndef TOSHOBOE_H
#define TOSHOBOE_H

/* Registers */
/*Receive and transmit task registers (read only) */
#define OBOE_RCVT	(0x00+(self->base))
#define OBOE_XMTT	(0x01+(self->base))
#define OBOE_XMTT_OFFSET	0x40

/*Page pointers to the TaskFile structure */
#define OBOE_TFP2	(0x02+(self->base))
#define OBOE_TFP0	(0x04+(self->base))
#define OBOE_TFP1	(0x05+(self->base))

/*Dunno */
#define OBOE_REG_3	(0x03+(self->base))

/*Number of tasks to use in Xmit and Recv queues */
#define OBOE_NTR	(0x07+(self->base))
#define OBOE_NTR_XMIT4	0x00
#define OBOE_NTR_XMIT8	0x10
#define OBOE_NTR_XMIT16	0x30
#define OBOE_NTR_XMIT32	0x70
#define OBOE_NTR_XMIT64	0xf0
#define OBOE_NTR_RECV4	0x00
#define OBOE_NTR_RECV8	0x01
#define OBOE_NTR_RECV6	0x03
#define OBOE_NTR_RECV32	0x07
#define OBOE_NTR_RECV64	0x0f

/* Dunno */
#define OBOE_REG_9	(0x09+(self->base))

/* Interrupt Status Register */
#define OBOE_ISR	(0x0c+(self->base))
#define OBOE_ISR_TXDONE	0x80
#define OBOE_ISR_RXDONE	0x40
#define OBOE_ISR_20	0x20
#define OBOE_ISR_10	0x10
#define OBOE_ISR_8	0x08         /*This is collision or parity or something */
#define OBOE_ISR_4	0x08
#define OBOE_ISR_2	0x08
#define OBOE_ISR_1	0x08

/*Dunno */
#define OBOE_REG_D	(0x0d+(self->base))

/*Register Lock Register */
#define OBOE_LOCK	((self->base)+0x0e)



/*Speed control registers */
#define OBOE_PMDL	(0x10+(self->base))
#define OBOE_PMDL_SIR	0x18
#define OBOE_PMDL_MIR	0xa0
#define OBOE_PMDL_FIR	0x40

#define OBOE_SMDL	(0x18+(self->base))
#define OBOE_SMDL_SIR	0x20
#define OBOE_SMDL_MIR	0x01
#define OBOE_SMDL_FIR	0x0f

#define OBOE_UDIV	(0x19+(self->base))

/*Dunno */
#define OBOE_REG_11	(0x11+(self->base))

/*Chip Reset Register */
#define OBOE_RST	(0x15+(self->base))
#define OBOE_RST_WRAP	0x8

/*Dunno */
#define OBOE_REG_1A	(0x1a+(self->base))
#define OBOE_REG_1B	(0x1b+(self->base))

/* The PCI ID of the OBOE chip */
#ifndef PCI_DEVICE_ID_FIR701
#define PCI_DEVICE_ID_FIR701 	0x0701
#endif

typedef unsigned int dword;
typedef unsigned short int word;
typedef unsigned char byte;
typedef dword Paddr;

struct OboeTask
  {
    __u16 len;
    __u8 unused;
    __u8 control;
    __u32 buffer;
  };

#define OBOE_NTASKS 64

struct OboeTaskFile
  {
    struct OboeTask recv[OBOE_NTASKS];
    struct OboeTask xmit[OBOE_NTASKS];
  };

#define OBOE_TASK_BUF_LEN (sizeof(struct OboeTaskFile) << 1)

/*These set the number of slots in use */
#define TX_SLOTS	4
#define RX_SLOTS	4

/* You need also to change this, toshiba uses 4,8 and 4,4 */
/* It makes no difference if you are only going to use ONETASK mode */
/* remember each buffer use XX_BUF_SZ more _PHYSICAL_ memory */
#define OBOE_NTR_VAL 	(OBOE_NTR_XMIT4 | OBOE_NTR_RECV4)

struct toshoboe_cb
  {
    struct net_device *netdev;      /* Yes! we are some kind of netdevice */
    struct net_device_stats stats;

    struct irlap_cb    *irlap;  /* The link layer we are binded to */
    struct qos_info     qos;    /* QoS capabilities for this device */

    chipio_t io;                /* IrDA controller information */

    __u32 flags;                /* Interface flags */
    __u32 new_speed;

    struct pci_dev *pdev;       /*PCI device */
    int base;                   /*IO base */
    int txpending;              /*how many tx's are pending */
    int txs, rxs;               /*Which slots are we at  */
    void *taskfilebuf;          /*The unaligned taskfile buffer */
    struct OboeTaskFile *taskfile;  /*The taskfile   */
    void *xmit_bufs[TX_SLOTS];  /*The buffers   */
    void *recv_bufs[RX_SLOTS];
    int open;
    int stopped;		/*Stopped by some or other APM stuff*/
  };


#endif


