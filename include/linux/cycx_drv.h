/*
* cycx_drv.h	CYCX Support Module.  Kernel API Definitions.
*
* Author:	Arnaldo Carvalho de Melo <acme@conectiva.com.br>
*
* Copyright:	(c) 1998-2000 Arnaldo Carvalho de Melo
*
* Based on sdladrv.h by Gene Kozin <genek@compuserve.com>
*
*		This program is free software; you can redistribute it and/or
*		modify it under the terms of the GNU General Public License
*		as published by the Free Software Foundation; either version
*		2 of the License, or (at your option) any later version.
* ============================================================================
* 1999/10/23	acme		cycxhw_t cleanup
* 1999/01/03	acme		more judicious use of data types...
*				uclong, ucchar, etc deleted, the u8, u16, u32
*				types are the portable way to go.
* 1999/01/03	acme		judicious use of data types... u16, u32, etc
* 1998/12/26	acme	 	FIXED_BUFFERS, CONF_OFFSET,
*                               removal of cy_read{bwl}
* 1998/08/08	acme	 	Initial version.
*/
#ifndef	_CYCX_DRV_H
#define	_CYCX_DRV_H

#define	CYCX_WINDOWSIZE	0x4000	/* default dual-port memory window size */
#define	GEN_CYCX_INTR	0x02
#define	RST_ENABLE	0x04
#define	START_CPU	0x06
#define	RST_DISABLE	0x08
#define	FIXED_BUFFERS	0x08
#define	TEST_PATTERN	0xaa55
#define	CMD_OFFSET	0x20
#define CONF_OFFSET     0x0380
#define	RESET_OFFSET	0x3c00	/* For reset file load */
#define	DATA_OFFSET	0x0100	/* For code and data files load */
#define	START_OFFSET	0x3ff0	/* 80186 starts here */

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/* Data Structures */
/* Adapter hardware configuration. Pointer to this structure is passed to all
 * APIs.  */
typedef struct cycxhw {
	u32 fwid;		/* firmware ID */
	int irq;		/* interrupt request level */
	u32 dpmbase;		/* dual-port memory base */
	u32 dpmsize;		/* dual-port memory size */
	u32 reserved[5];
} cycxhw_t;

/* Function Prototypes */
extern int cycx_setup (cycxhw_t *hw, void *sfm, u32 len);
extern int cycx_down (cycxhw_t *hw);
extern int cycx_peek (cycxhw_t *hw, u32 addr, void *buf, u32 len);
extern int cycx_poke (cycxhw_t *hw, u32 addr, void *buf, u32 len);
extern int cycx_exec (u32 addr);

extern void cycx_inten (cycxhw_t *hw);
extern void cycx_intr (cycxhw_t *hw);
#endif	/* _CYCX_DRV_H */
