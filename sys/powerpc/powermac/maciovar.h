/*
 * Copyright 2002 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _MACIO_MACIOVAR_H_
#define _MACIO_MACIOVAR_H_

/* 
 * Accessors for macio devices
 */

enum macio_ivars {
        MACIO_IVAR_NODE,
        MACIO_IVAR_NAME,
	MACIO_IVAR_DEVTYPE,
        MACIO_IVAR_NREGS,
        MACIO_IVAR_REGS,
};

#define MACIO_ACCESSOR(var, ivar, type)                                 \
        __BUS_ACCESSOR(macio, var, MACIO, ivar, type)

MACIO_ACCESSOR(node,            NODE,                   phandle_t)
MACIO_ACCESSOR(name,            NAME,                   char *)
MACIO_ACCESSOR(devtype,         DEVTYPE,                char *)
MACIO_ACCESSOR(nregs,           NREGS,                  u_int)
MACIO_ACCESSOR(regs,            REGS,                   struct macio_reg *)

#undef MACIO_ACCESSOR

/*
 * The addr space size
 * XXX it would be better if this could be determined by querying the
 *     PCI device, but there isn't an access method for this
 */
#define MACIO_REG_SIZE  0x7ffff

/*
 * Macio softc
 */
struct macio_softc {
	phandle_t    sc_node;
	vm_offset_t  sc_base;
	vm_offset_t  sc_size;
	struct rman  sc_mem_rman;
};

/*
 * Format of a macio reg property entry.
 */
struct macio_reg {
	u_int32_t	mr_base;
	u_int32_t	mr_size;
};

/*
 * Per macio device structure.
 */
struct macio_devinfo {
	phandle_t  mdi_node;
	char      *mdi_name;
	char      *mdi_device_type;
	int        mdi_interrupts[5];
	int	   mdi_ninterrupts;
	int        mdi_base;
	int        mdi_nregs;
	struct macio_reg *mdi_regs;
	struct resource_list mdi_resources;
};

#endif /* _MACIO_MACIOVAR_H_ */
