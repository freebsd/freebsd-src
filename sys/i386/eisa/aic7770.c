/*
 * Product specific probe and attach routines for:
 * 	27/284X and aic7770 motherboard SCSI controllers
 *
 * Copyright (c) 1995 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Justin T. Gibbs.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <sys/devconf.h>
#include <machine/cpufunc.h>
#include <i386/scsi/aic7xxx.h>

int	aic7770probe __P((struct isa_device *dev)); 
int	aic7770_attach __P((struct isa_device *dev));

/* 
 * Standard EISA Host ID regs  (Offset from slot base)
 */

#define HID0		0xC80	/* 0,1: msb of ID2, 2-7: ID1      */
#define HID1		0xC81	/* 0-4: ID3, 5-7: LSB ID2         */ 
#define HID2		0xC82	/* product                        */ 
#define HID3		0xC83	/* firmware revision              */

#define CHAR1(B1,B2) (((B1>>2) & 0x1F) | '@')
#define CHAR2(B1,B2) (((B1<<3) & 0x18) | ((B2>>5) & 0x7)|'@')
#define CHAR3(B1,B2) ((B2 & 0x1F) | '@')

#define	MAX_SLOTS	16	/* max slots on the EISA bus */
static	ahc_slot = 0;		/* slot last board was found in */

struct isa_driver ahcdriver = {aic7770probe, aic7770_attach, "ahc"};

typedef struct
{
  ahc_type type;
  unsigned char id; /* The Last EISA Host ID reg */
} aic7770_sig;

static struct kern_devconf kdc_aic7770[NAHC] = { {
	0, 0, 0,                /* filled in by dev_attach */
	"ahc", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_BUSY,		/* host adapters are always ``in use'' */
	"Adaptec aic7770 based SCSI host adapter"
} };

static inline void
aic7770_registerdev(struct isa_device *id)
{
	if(id->id_unit)
		kdc_aic7770[id->id_unit] = kdc_aic7770[0];
	kdc_aic7770[id->id_unit].kdc_unit = id->id_unit;
	kdc_aic7770[id->id_unit].kdc_parentdata = id;
	dev_attach(&kdc_aic7770[id->id_unit]);
}  

int
aic7770probe(struct isa_device *dev)
{       
        u_long  port;
	int	i;
        u_char  sig_id[4];

	aic7770_sig valid_ids[] = {
	/* Entries of other tested adaptors should be added here */
		  AHC_274, 0x71,  /*274x, Card*/
		  AHC_274, 0x70,  /*274x, Motherboard*/
		  AHC_284, 0x56,  /*284x, BIOS enabled*/
		  AHC_284, 0x57,  /*284x, BIOS disabled*/
	};


        ahc_slot++;
        while (ahc_slot <= MAX_SLOTS) {
                port = 0x1000 * ahc_slot;
		for( i = 0; i < sizeof(sig_id); i++ )
		{
		       /* 
			* An outb is required to prime these registers on
			* VL cards
			*/
		        outb( port + HID0, HID0 + i );
                        sig_id[i] = inb(port + HID0 + i);
		}
                if (sig_id[0] == 0xff) {
                        ahc_slot++;
                        continue;
                }
		/* Check manufacturer's ID. */
		if ((CHAR1(sig_id[0], sig_id[1]) == 'A')
		    && (CHAR2(sig_id[0], sig_id[1]) == 'D')
		    && (CHAR3(sig_id[0], sig_id[1]) == 'P')
		    && (sig_id[2] == 0x77)) {
			for(i=0; i < sizeof(valid_ids)/sizeof(aic7770_sig);i++)
                        	if ( sig_id[3] == valid_ids[i].id ) {
					int unit = dev->id_unit;
		                        dev->id_iobase = port;
               			        if(ahcprobe(unit, port, 
						  valid_ids[i].type)){ 
					        /*
					         * If it's there, put in it's 
						 * interrupt vectors
					         */
					        dev->id_irq = (1 << 
							ahcdata[unit]->vect);
					        dev->id_drq = -1; /* EISA dma */
				        	ahc_unit++;
					        return IO_EISASIZE;
	                   		}
				}
		}
                ahc_slot++;
        }
        return 0;
}

int
aic7770_attach(dev)
        struct isa_device *dev;
{
        int     unit = dev->id_unit;
        aic7770_registerdev(dev);
	return ahc_attach(unit);
}

