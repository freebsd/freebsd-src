/*
 * EISA bus device definitions
 *
 * Copyright (c) 1995 Justin T. Gibbs.
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
 *
 *	$Id: eisaconf.h,v 1.3 1995/11/05 04:42:50 gibbs Exp $
 */

#ifndef _I386_EISA_EISACONF_H_
#define _I386_EISA_EISACONF_H_ 1

#define EISA_SLOTS 10   /* PCI clashes with higher ones.. fix later */

#define EISA_MFCTR_CHAR0(ID) (char)(((ID>>26) & 0x1F) | '@')  /* Bits 26-30 */
#define EISA_MFCTR_CHAR1(ID) (char)(((ID>>21) & 0x1F) | '@')  /* Bits 21-25 */
#define EISA_MFCTR_CHAR2(ID) (char)(((ID>>16) & 0x1F) | '@')  /* Bits 16-20 */
#define EISA_MFCTR_ID(ID)    (short)((ID>>16) & 0xFF)	      /* Bits 16-31 */
#define EISA_PRODUCT_ID(ID)  (short)((ID>>4)  & 0xFFF)        /* Bits  4-15 */
#define EISA_REVISION_ID(ID) (u_char)(ID & 0x0F)              /* Bits  0-3  */

extern struct linker_set eisadriver_set;

typedef u_long eisa_id_t;   /* Should use u_int32? */

struct eisa_ioconf {
	int	slot;
        u_long	iobase;      /* base i/o address */
        int	iosize;      /* size of i/o space */
        u_short irq;         /* interrupt request */
        caddr_t maddr;       /* physical i/o memory address on bus (if any)*/
        int     msize;       /* size of i/o memory */
};

struct kern_devconf;
struct eisa_device;

struct eisa_driver {
	char*	name;			/* device name */
	int	(*probe) __P((void));
					/* test whether device is present */
	int	(*attach) __P((struct eisa_device *));
					/* setup driver for a device */
	int	(*shutdown) __P((struct kern_devconf *, int));
					/* Return the device to a safe
					 * state before shutdown
					 */
	u_long  *unit;			/* Next availible unit */
};

/* To be replaced by the "super device" generic device structure... */
struct eisa_device {
	eisa_id_t		id;
	u_long			unit;
	char*			full_name; /* for use in the probe message */
	struct eisa_ioconf	ioconf;
	struct eisa_driver*	driver;
	struct kern_devconf*	kdc;
};

struct eisa_device *eisa_match_dev __P((struct eisa_device *, char * (*)(eisa_id_t)));

void eisa_reg_start __P((struct eisa_device *));
void eisa_reg_end __P((struct eisa_device *));
int eisa_add_intr __P((struct eisa_device *, int));
int eisa_reg_intr __P((struct eisa_device *, int, void (*)(void *), void *, u_int *, int));
int eisa_release_intr __P((struct eisa_device *, int, void (*)(void *)));
int eisa_enable_intr __P((struct eisa_device *, int));
int eisa_add_iospace __P((struct eisa_device *, u_long, int));
int eisa_reg_iospace __P((struct eisa_device *, u_long, int));
int eisa_registerdev __P((struct eisa_device *, struct eisa_driver *, struct kern_devconf *));


extern int eisa_externalize __P((struct eisa_device *, void *, size_t *));

extern int eisa_generic_externalize __P((struct proc *,struct kern_devconf *, void *, size_t));
extern struct kern_devconf kdc_eisa0;

#define EISA_EXTERNALLEN (sizeof(struct eisa_device))

#endif /* _I386_EISA_EISACONF_H_ */
