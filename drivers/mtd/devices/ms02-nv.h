/*
 *      Copyright (c) 2001 Maciej W. Rozycki
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/ioport.h>
#include <linux/mtd/mtd.h>

/* MS02-NV iomem register offsets. */
#define MS02NV_CSR		0x400000	/* control & status register */

/* MS02-NV memory offsets. */
#define MS02NV_DIAG		0x0003f8	/* diagnostic status */
#define MS02NV_MAGIC		0x0003fc	/* MS02-NV magic ID */
#define MS02NV_RAM		0x000400	/* general-purpose RAM start */

/* MS02-NV diagnostic status constants. */
#define MS02NV_DIAG_SIZE_MASK	0xf0		/* RAM size mask */
#define MS02NV_DIAG_SIZE_SHIFT	0x10		/* RAM size shift (left) */

/* MS02-NV general constants. */
#define MS02NV_ID		0x03021966	/* MS02-NV magic ID value */
#define MS02NV_SLOT_SIZE	0x800000	/* size of the address space
						   decoded by the module */

typedef volatile u32 ms02nv_uint;

struct ms02nv_private {
	struct mtd_info *next;
	struct {
		struct resource *module;
		struct resource *diag_ram;
		struct resource *user_ram;
		struct resource *csr;
	} resource;
	u_char *addr;
	size_t size;
	u_char *uaddr;
};
