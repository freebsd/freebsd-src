/*
 * Written by Billie Alsup (balsup@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5)and OSF/1 operating
 * systems.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * $Id: eisaconf.c,v 1.1 1995/03/13 09:10:17 root Exp root $
 */

/*
 * Ported to run under FreeBSD by Julian Elischer (julian@tfs.com) Sept 1992
 */


#include <sys/param.h>
#include <sys/systm.h>          /* isn't it a joy */
#include <sys/kernel.h>         /* to have three of these */
#include <sys/conf.h>

#include "sys/types.h"
#include "i386/isa/icu.h"
#include "i386/isa/isa_device.h" /*we're a superset, so we need this */
#include "eisaconf.h"


struct isa_device eisaSlot[EISA_SLOTS];
struct isa_device isa_devtab_eisa[EISA_SLOTS+1];
int nexttab = 0;
extern struct eisa_dev eisa_dev[];

#define EISA_MAKEID(p) ((((p)[0]&0x1F)<<10)|(((p)[1]&0x1F)<<5)|(((p)[2]&0x1F)))
#define EISA_ID0(i) ((((i)>>10)&0x1F)+0x40)
#define EISA_ID1(i) ((((i)>>5)&0x1F)+0x40)
#define EISA_ID2(i) (((i)&0x1F)+0x40)
/*
** probe for EISA devices
*/
void
eisa_configure()
{
    int i,j,slot,found,numports;
    unsigned int checkthese;
    struct eisa_dev *edev_p;
    int eisaBase = 0xC80;
    unsigned short productID, productType;
    unsigned char productRevision,controlBits;
	static char hexdigit[] = "0123456789ABCDEF";
#define HEXDIGIT(i) hexdigit[(i)&0x0f]

    outb(eisaBase,0xFF);
    productID = inb(eisaBase);
    if (productID & 0x80) {
      printf("Warning: running EISA kernel on non-EISA system board\n");
      return;
    } 
    printf("Probing for devices on EISA bus\n");
    productID = (productID<<8) | inb(eisaBase+1);
    productRevision = inb(eisaBase+2);

    printf("EISA0: %c%c%c v%d (System Board)\n"
                ,EISA_ID0(productID)
                ,EISA_ID1(productID)
                ,EISA_ID2(productID)
                ,(productRevision&7));

    for (slot=1; eisaBase += 0x1000, slot < EISA_SLOTS; slot++) {
      outb(eisaBase,0xFF);
      productID = inb(eisaBase);
      if (productID & 0x80) continue;  /* no EISA card in slot */

      productID = (productID<<8) | inb(eisaBase+1);
      productType = inb(eisaBase+2);
      productRevision = inb(eisaBase+3);
      productType = (productType<<4) | (productRevision>>4);
      productRevision &= 15;
      controlBits = inb(eisaBase+4);

      printf("EISA%d: %c%c%c-%c%c%c.%x\n"
        ,slot,EISA_ID0(productID),EISA_ID1(productID),EISA_ID2(productID)
        ,HEXDIGIT(productType>>8)
		,HEXDIGIT(productType>>4)
		,HEXDIGIT(productType)
		,productRevision);

      if (!(controlBits & 1)) {
        printf("...Card is disabled\n");
		/* continue;*/
      }

      /*
      ** See if we recognize this product
      */

      for (edev_p = eisa_dev,found=0; edev_p->productID[0]; edev_p++) {
        struct isa_device *dev_p;
        struct  isa_driver  *drv_p;
        unsigned short configuredID;

        configuredID = EISA_MAKEID(edev_p->productID);
        if (configuredID != productID) continue;
        if (edev_p->productType != productType) continue;
        if (edev_p->productRevision > productRevision) continue;

        /*
        ** we're assuming:
        **      if different drivers for the same board exist
        **      (due to some revision incompatibility), that the
        **      drivers will be listed in descending revision
        **      order.  The revision in the eisaDevs structure
        **      should indicate the lowest revision supported
        **      by the code.
        **
        */
        dev_p = &eisaSlot[slot];
        memcpy(dev_p,&edev_p->isa_dev,sizeof(edev_p->isa_dev));

        drv_p = dev_p->id_driver;
        dev_p->id_iobase = eisaBase; /* may get ammended by driver */

#if defined(DEBUG)
        printf("eisaProbe: probing %s%d\n"
                ,drv_p->driver_name, dev_p->id_unit);
#endif /* defined(DEBUG) */

        if (!(numports = drv_p->probe(dev_p))) {
            continue;  /* try another eisa device */
        }
        edev_p->isa_dev.id_unit++; /*dubious*/
/** this should all be put in some common routine **/
	printf("%s%d", drv_p->name, dev_p->id_unit);
	if (numports != -1) {
		printf(" at 0x%x", dev_p->id_iobase);
		if ((dev_p->id_iobase + numports - 1) != dev_p->id_iobase) {
			printf("-0x%x", dev_p->id_iobase + numports - 1);
		}
	}

	if (dev_p->id_irq)
		printf(" irq %d", ffs(dev_p->id_irq) - 1);
	if (dev_p->id_drq != -1)
		printf(" drq %d", dev_p->id_drq);
	if (dev_p->id_maddr)
		printf(" maddr 0x%lx", kvtop(dev_p->id_maddr));
	if (dev_p->id_msize)
		printf(" msize %d", dev_p->id_msize);
	if (dev_p->id_flags)
		printf(" flags 0x%x", dev_p->id_flags);
	if (dev_p->id_iobase) {
		if (dev_p->id_iobase < 0x100) {
			printf(" on motherboard\n");
		} else {
			if (dev_p->id_iobase >= 0x1000) {
				printf (" on EISA\n");
			} else {
				printf (" on ISA emulation\n");
			}
		}
	}
        /*
        ** Now look for any live devices with the same starting I/O port and
	** give up if we clash
        **
        ** what i'd really like is to set is how many i/o ports are in use.
        ** but that isn't in this structure...
        **
        */
	checkthese = 0;
	if(dev_p->id_iobase )	checkthese |= CC_IOADDR;
	if(dev_p->id_drq != -1 ) checkthese |= CC_DRQ;
	if(dev_p->id_irq )	checkthese |= CC_IRQ;
	if(dev_p->id_maddr )	checkthese |= CC_MEMADDR;
	/* this may be stupid, it's probably too late if we clash here */
	if(haveseen_isadev( dev_p,checkthese))
		break;	/* we can't proceed due to collision. bail */
	/* mark ourselves in existence and then put us in the eisa list */
	/* so that other things check against US for a clash */
        dev_p->id_alive = (numports == -1? 1 : numports);
	memcpy(&(isa_devtab_eisa[nexttab]),dev_p,sizeof(edev_p->isa_dev));
        drv_p->attach(dev_p);

	if (dev_p->id_irq) {
		if (edev_p->imask)
			INTRMASK(*(edev_p->imask), dev_p->id_irq);
		register_intr(ffs(dev_p->id_irq) - 1, dev_p->id_id,
			dev_p->id_ri_flags, dev_p->id_intr,
			edev_p->imask, dev_p->id_unit);
		INTREN(dev_p->id_irq);
	}
	found = 1;
	nexttab++;
        break; /* go look at next slot*/
     }/* end of loop on known devices */
     if (!found) {
      printf("...No driver installed for board\n");
     }
  }/* end of loop on slots */
}/* end of routine */





