/*	$Id: cardbusvar.h,v 1.1.2.1 1999/02/16 16:46:08 haya Exp $	*/

/*
 * Copyright (c) 1998 HAYAKAWA Koichi.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the author.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* $FreeBSD: src/sys/dev/cardbus/cardbusvar.h,v 1.1 1999/11/18 07:21:51 imp Exp $ */

#if !defined SYS_DEV_PCCARD_CARDBUSVAR_H
#define SYS_DEV_PCCARD_CARDBUSVAR_H
#include <pci/pcivar.h> /* XXX */
typedef pcitag_t cardbustag_t; /* XXX */

typedef struct cardbus_functions {
    int (*cardbus_ctrl) __P((cardbus_chipset_tag_t, int));
    int (*cardbus_power) __P((cardbus_chipset_tag_t, int));
    int (*cardbus_mem_open) __P((cardbus_chipset_tag_t, int, u_int32_t, u_int32_t));
    int (*cardbus_mem_close) __P((cardbus_chipset_tag_t, int));
    int (*cardbus_io_open) __P((cardbus_chipset_tag_t, int, u_int32_t, u_int32_t));
    int (*cardbus_io_close) __P((cardbus_chipset_tag_t, int));
    void *(*cardbus_intr_establish) __P((cardbus_chipset_tag_t, int irq, int level, int (*ih)(void *), void *sc));
    void (*cardbus_intr_disestablish) __P((cardbus_chipset_tag_t ct, void *ih));
    cardbustag_t (*cardbus_make_tag) __P((cardbus_chipset_tag_t, int, int, int));  
    void (*cardbus_free_tag) __P((cardbus_chipset_tag_t, cardbustag_t));
    cardbusreg_t (*cardbus_conf_read) __P((cardbus_chipset_tag_t, cardbustag_t, int));
    void (*cardbus_conf_write) __P((cardbus_chipset_tag_t, cardbustag_t, int, cardbusreg_t));
} cardbus_function_t, *cardbus_function_tag_t;

/**********************************************************************
* struct cbslot_attach_args is the attach argument for cardbus slot.
**********************************************************************/
struct cbslot_attach_args {
  char *cba_busname;
  bus_space_tag_t cba_iot;	/* cardbus i/o space tag */
  bus_space_tag_t cba_memt;	/* cardbus mem space tag */
  bus_dma_tag_t cba_dmat;	/* DMA tag */

  int cba_bus;			/* cardbus bus number */
  int cba_function;		/* slot number on this Host Bus Adaptor */

  cardbus_chipset_tag_t cba_cc;	/* cardbus chipset */
  cardbus_function_tag_t cba_cf; /* cardbus functions */
  int cba_intrline;		/* interrupt line */
};

#define cbslotcf_slot  cf_loc[0]
#define CBSLOT_UNK_SLOT -1

/**********************************************************************
* struct cardslot_if is the interface for cardslot.
**********************************************************************/
struct cardslot_if {
    struct device *(*if_card_attach) __P((struct cardbus_softc*));
};
/**********************************************************************
* struct cardbus_softc is the softc for cardbus card.
**********************************************************************/
struct cardbus_softc {
  struct device sc_dev;		/* fundamental device structure */

  int sc_bus;			/* cardbus bus number */
  int sc_device;		/* cardbus device number */
  int sc_intrline;		/* CardBus intrline */
  
  bus_space_tag_t sc_iot;	/* CardBus I/O space tag */
  bus_space_tag_t sc_memt;	/* CardBus MEM space tag */
  bus_dma_tag_t sc_dmat;	/* DMA tag */
  cardbus_chipset_tag_t sc_cc;	/* CardBus chipset */
  cardbus_function_tag_t sc_cf;	/* CardBus function */

  int sc_volt;			/* applied Vcc voltage */
#define PCCARD_33V  0x02
#define PCCARD_XXV  0x04
#define PCCARD_YYV  0x08
  struct cardslot_if sc_if;  
};
void
cardslot_if_setup __P((struct cardbus_softc*));

/**********************************************************************
* struct cbslot_attach_args is the attach argument for cardbus card.
**********************************************************************/
struct cardbus_attach_args {
  int ca_unit;
  cardbus_chipset_tag_t ca_cc;
  cardbus_function_tag_t ca_cf;

  bus_space_tag_t ca_iot;	/* CardBus I/O space tag */
  bus_space_tag_t ca_memt;	/* CardBus MEM space tag */
  bus_dma_tag_t ca_dmat;	/* DMA tag */

  u_int ca_device;
  u_int ca_function;
  cardbustag_t ca_tag;
  cardbusreg_t ca_id;
  cardbusreg_t ca_class;

  /* interrupt information */
  cardbus_intr_line_t ca_intrline;
};


#define CARDBUS_ENABLE  1	/* enable the channel */
#define CARDBUS_DISABLE 2	/* disable the channel */
#define CARDBUS_RESET   4
#define CARDBUS_CD      7
#  define CARDBUS_NOCARD 0
#  define CARDBUS_5V_CARD 0x01	/* XXX: It must not exist */
#  define CARDBUS_3V_CARD 0x02
#  define CARDBUS_XV_CARD 0x04
#  define CARDBUS_YV_CARD 0x08
#define CARDBUS_IO_ENABLE    100
#define CARDBUS_IO_DISABLE   101
#define CARDBUS_MEM_ENABLE   102
#define CARDBUS_MEM_DISABLE  103
#define CARDBUS_BM_ENABLE    104 /* bus master */
#define CARDBUS_BM_DISABLE   105

#define CARDBUS_VCC_UC  0x0000
#define CARDBUS_VCC_3V  0x0001
#define CARDBUS_VCC_XV  0x0002
#define CARDBUS_VCC_YV  0x0003
#define CARDBUS_VCC_0V  0x0004
#define CARDBUS_VCC_5V  0x0005	/* ??? */
#define CARDBUS_VCCMASK 0x000f
#define CARDBUS_VPP_UC  0x0000
#define CARDBUS_VPP_VCC 0x0010
#define CARDBUS_VPP_12V 0x0030
#define CARDBUS_VPP_0V  0x0040
#define CARDBUS_VPPMASK 0x00f0


/**********************************************************************
* Locators devies that attach to 'cardbus', as specified to config.
**********************************************************************/
#include "locators.h"

#define cardbuscf_dev cf_loc[CARDBUSCF_DEV]
#define CARDBUS_UNK_DEV CARDBUSCF_DEV_DEFAULT

#define cardbuscf_function cf_loc[CARDBUSCF_FUNC]
#define CARDBUS_UNK_FUNCTION CARDBUSCF_FUNC_DEFAULT

struct device *cardbus_attach_card __P((struct cardbus_softc *));
void *cardbus_intr_establish __P((cardbus_chipset_tag_t, cardbus_function_tag_t, cardbus_intr_handle_t irq, int level, int (*func) (void *), void *arg));
void cardbus_intr_disestablish __P((cardbus_chipset_tag_t, cardbus_function_tag_t, void *handler));

#define cardbus_conf_read(cc, cf, tag, offs) ((cf)->cardbus_conf_read)((cc), (tag), (offs))
#define cardbus_conf_write(cc, cf, tag, offs, val) ((cf)->cardbus_conf_write)((cc), (tag), (offs), (val))
#define cardbus_make_tag(cc, cf, bus, device, function) ((cf)->cardbus_make_tag)((cc), (bus), (device), (function))
#define cardbus_free_tag(cc, cf, tag) ((cf)->cardbus_free_tag)((cc), (tag))

#endif /* SYS_DEV_PCCARD_CARDBUSVAR_H */

