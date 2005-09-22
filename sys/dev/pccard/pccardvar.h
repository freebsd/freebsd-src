/*	$NetBSD: pcmciavar.h,v 1.12 2000/02/08 12:51:31 enami Exp $	*/
/* $FreeBSD$ */

/*-
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCCARD_API_LEVEL.  When set to 5, we provide a 5.x compatable API
 * for driver writers that have to share their code between 5.x and 6.x.
 * The 5.x compatibility interfaces will be unsupported in 7.0, at which
 * point we'll only support 6 and newer, etc.
 */
#ifndef PCCARD_API_LEVEL
#define PCCARD_API_LEVEL 6
#elif PCCARD_API_LEVEL < 5
#error "pccard API less than 5 unsupported"
#endif

/*
 * Contains information about mapped/allocated i/o spaces.
 */
struct pccard_io_handle {
	bus_space_tag_t iot;		/* bus space tag (from chipset) */
	bus_space_handle_t ioh;		/* mapped space handle */
	bus_addr_t      addr;		/* resulting address in bus space */
	bus_size_t      size;		/* size of i/o space */
	int             flags;		/* misc. information */
	int		width;
};

#define	PCCARD_IO_ALLOCATED	0x01	/* i/o space was allocated */

/*
 * Contains information about allocated memory space.
 */
struct pccard_mem_handle {
	bus_space_tag_t memt;		/* bus space tag (from chipset) */
	bus_space_handle_t memh;	/* mapped space handle */
	bus_addr_t      addr;		/* resulting address in bus space */
	bus_size_t      size;		/* size of mem space */
	bus_size_t      realsize;	/* how much we really allocated */
	bus_addr_t	cardaddr;	/* Absolute address on card */
	int		kind;
};

#define	PCCARD_WIDTH_AUTO	0
#define	PCCARD_WIDTH_IO8	1
#define	PCCARD_WIDTH_IO16	2

struct pccard_tuple {
	unsigned int	code;
	unsigned int	length;
	u_long		mult;		/* dist btn successive bytes */
	bus_addr_t	ptr;
	bus_space_tag_t	memt;
	bus_space_handle_t memh;
};

typedef int (*pccard_scan_t)(const struct pccard_tuple *, void *);

struct pccard_product {
	const char	*pp_name;		/* NULL if end of table */
#define PCCARD_VENDOR_ANY (0xffffffff)
	uint32_t	pp_vendor;
#define PCCARD_PRODUCT_ANY (0xffffffff)
	uint32_t	pp_product;
	const char	*pp_cis[4];
};

typedef int (*pccard_product_match_fn) (device_t dev,
    const struct pccard_product *ent, int vpfmatch);

#include "card_if.h"

/*
 * make this inline so that we don't have to worry about dangling references
 * to it in the modules or the code.
 */
static __inline const struct pccard_product *
pccard_product_lookup(device_t dev, const struct pccard_product *tab,
    size_t ent_size, pccard_product_match_fn matchfn)
{
	return CARD_DO_PRODUCT_LOOKUP(device_get_parent(dev), dev,
	    tab, ent_size, matchfn);
}

#define	pccard_cis_read_1(tuple, idx0)					\
	(bus_space_read_1((tuple)->memt, (tuple)->memh, (tuple)->mult*(idx0)))

#define	pccard_tuple_read_1(tuple, idx1)				\
	(pccard_cis_read_1((tuple), ((tuple)->ptr+(2+(idx1)))))

#define	pccard_tuple_read_2(tuple, idx2)				\
	(pccard_tuple_read_1((tuple), (idx2)) |				\
	 (pccard_tuple_read_1((tuple), (idx2)+1)<<8))

#define	pccard_tuple_read_3(tuple, idx3)				\
	(pccard_tuple_read_1((tuple), (idx3)) |				\
	 (pccard_tuple_read_1((tuple), (idx3)+1)<<8) |			\
	 (pccard_tuple_read_1((tuple), (idx3)+2)<<16))

#define	pccard_tuple_read_4(tuple, idx4)				\
	(pccard_tuple_read_1((tuple), (idx4)) |				\
	 (pccard_tuple_read_1((tuple), (idx4)+1)<<8) |			\
	 (pccard_tuple_read_1((tuple), (idx4)+2)<<16) |			\
	 (pccard_tuple_read_1((tuple), (idx4)+3)<<24))

#define	pccard_tuple_read_n(tuple, n, idxn)				\
	(((n)==1)?pccard_tuple_read_1((tuple), (idxn)) :		\
	 (((n)==2)?pccard_tuple_read_2((tuple), (idxn)) :		\
	  (((n)==3)?pccard_tuple_read_3((tuple), (idxn)) :		\
	   /* n == 4 */ pccard_tuple_read_4((tuple), (idxn)))))

#define	PCCARD_SPACE_MEMORY	1
#define	PCCARD_SPACE_IO		2

#define	pccard_mfc(sc)							\
		(STAILQ_FIRST(&(sc)->card.pf_head) &&			\
		 STAILQ_NEXT(STAILQ_FIRST(&(sc)->card.pf_head),pf_list))

/* Convenience functions */

static __inline int
pccard_cis_scan(device_t dev, pccard_scan_t fct, void *arg)
{
	return (CARD_CIS_SCAN(device_get_parent(dev), dev, fct, arg));
}

static __inline int
pccard_attr_read_1(device_t dev, uint32_t offset, uint8_t *val)
{
	return (CARD_ATTR_READ(device_get_parent(dev), dev, offset, val));
}

static __inline int
pccard_attr_write_1(device_t dev, uint32_t offset, uint8_t val)
{
	return (CARD_ATTR_WRITE(device_get_parent(dev), dev, offset, val));
}

static __inline int
pccard_ccr_read_1(device_t dev, uint32_t offset, uint8_t *val)
{
	return (CARD_CCR_READ(device_get_parent(dev), dev, offset, val));
}

static __inline int
pccard_ccr_write_1(device_t dev, uint32_t offset, uint8_t val)
{
	return (CARD_CCR_WRITE(device_get_parent(dev), dev, offset, val));
}

/* ivar interface */
enum {
	PCCARD_IVAR_ETHADDR,	/* read ethernet address from CIS tupple */
	PCCARD_IVAR_VENDOR,
	PCCARD_IVAR_PRODUCT,
	PCCARD_IVAR_PRODEXT,
	PCCARD_IVAR_FUNCTION_NUMBER,
	PCCARD_IVAR_VENDOR_STR,	/* CIS string for "Manufacturer" */
	PCCARD_IVAR_PRODUCT_STR,/* CIS strnig for "Product" */
	PCCARD_IVAR_CIS3_STR,
	PCCARD_IVAR_CIS4_STR,
	PCCARD_IVAR_FUNCTION
};

#define PCCARD_ACCESSOR(A, B, T)					\
__inline static int							\
pccard_get_ ## A(device_t dev, T *t)					\
{									\
	return BUS_READ_IVAR(device_get_parent(dev), dev,		\
	    PCCARD_IVAR_ ## B, (uintptr_t *) t);			\
}

PCCARD_ACCESSOR(ether,		ETHADDR,		uint8_t)
PCCARD_ACCESSOR(vendor,		VENDOR,			uint32_t)
PCCARD_ACCESSOR(product,	PRODUCT,		uint32_t)
PCCARD_ACCESSOR(prodext,	PRODEXT,		uint16_t)
PCCARD_ACCESSOR(function_number,FUNCTION_NUMBER,	uint32_t)
PCCARD_ACCESSOR(function,	FUNCTION,		uint32_t)
PCCARD_ACCESSOR(vendor_str,	VENDOR_STR,		const char *)
PCCARD_ACCESSOR(product_str,	PRODUCT_STR,		const char *)
PCCARD_ACCESSOR(cis3_str,	CIS3_STR,		const char *)
PCCARD_ACCESSOR(cis4_str,	CIS4_STR,		const char *)

/* shared memory flags */
enum {
	PCCARD_A_MEM_COM,       /* common */
	PCCARD_A_MEM_ATTR,      /* attribute */
	PCCARD_A_MEM_8BIT,      /* 8 bit */
	PCCARD_A_MEM_16BIT      /* 16 bit */
};

#define PCCARD_S(a, b) PCMCIA_STR_ ## a ## _ ## b
#define PCCARD_P(a, b) PCMCIA_PRODUCT_ ## a ## _ ## b
#define PCCARD_C(a, b) PCMCIA_CIS_ ## a ## _ ## b
#if PCCARD_API_LEVEL >= 6
#define PCMCIA_CARD_D(v, p) { PCCARD_S(v, p), PCMCIA_VENDOR_ ## v, \
		PCCARD_P(v, p), PCCARD_C(v, p) }
#define PCMCIA_CARD2_D(v1, p1, p2) \
		{ PCMCIA_STR_ ## p2, PCMCIA_VENDOR_ ## v1, PCCARD_P(v1, p1), \
		  PCMCIA_CIS_ ## p2}
#define PCMCIA_CARD(v, p) { NULL, PCMCIA_VENDOR_ ## v, \
		PCCARD_P(v, p), PCCARD_C(v, p) }
#define PCMCIA_CARD2(v1, p1, p2) \
		{ NULL, PCMCIA_VENDOR_ ## v1, PCCARD_P(v1, p1), \
		  PCMCIA_CIS_ ## p2}
#else
#define PCMCIA_CARD_D(v, p, f) { PCCARD_S(v, p), PCMCIA_VENDOR_ ## v, \
		PCCARD_P(v, p), PCCARD_C(v, p) }
#define PCMCIA_CARD2_D(v1, p1, p2, f) \
		{ PCMCIA_STR_ ## p2, PCMCIA_VENDOR_ ## v1, PCCARD_P(v1, p1), \
		  PCMCIA_CIS_ ## p2}
#define PCMCIA_CARD(v, p, f) { NULL, PCMCIA_VENDOR_ ## v, \
		PCCARD_P(v, p), PCCARD_C(v, p) }
#define PCMCIA_CARD2(v1, p1, p2, f) \
		{ NULL, PCMCIA_VENDOR_ ## v1, PCCARD_P(v1, p1), \
		  PCMCIA_CIS_ ## p2}
#endif
