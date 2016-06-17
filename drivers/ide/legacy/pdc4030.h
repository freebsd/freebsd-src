/*
 *  linux/drivers/ide/legacy/pdc4030.h
 *
 *  Copyright (C) 1995-1998  Linus Torvalds & authors
 */

/*
 * Principal author: Peter Denison <peterd@pnd-pc.demon.co.uk>
 */

#ifndef IDE_PROMISE_H
#define IDE_PROMISE_H

#include <linux/config.h>

#ifndef CONFIG_BLK_DEV_PDC4030
# ifdef _IDE_DISK

# define IS_PDC4030_DRIVE (0)	/* auto-NULLs out pdc4030 code */

ide_startstop_t promise_rw_disk(ide_drive_t *, struct request *, unsigned long);

ide_startstop_t promise_rw_disk(ide_drive_t *drive, struct request *rq, unsigned long block)
{
        return ide_stopped;
}
# endif /* _IDE_DISK */
#else /* CONFIG_BLK_DEV_PDC4030 */
# ifdef _IDE_DISK
#  define IS_PDC4030_DRIVE (HWIF(drive)->chipset == ide_pdc4030)

ide_startstop_t promise_rw_disk(ide_drive_t *, struct request *, unsigned long);

# endif /* _IDE_DISK */
#endif /* CONFIG_BLK_DEV_PDC4030 */

#ifdef __PROMISE_4030
#define	PROMISE_EXTENDED_COMMAND	0xF0
#define	PROMISE_READ			0xF2
#define	PROMISE_WRITE			0xF3
/* Extended commands - main command code = 0xf0 */
#define	PROMISE_GET_CONFIG		0x10
#define	PROMISE_IDENTIFY		0x20

struct translation_mode {
	u16	cyl;
	u8	head;
	u8	sect;
};

struct dc_ident {
	u8	type;
	u8	unknown1;
	u8	hw_revision;
	u8	firmware_major;
	u8	firmware_minor;
	u8	bios_address;
	u8	irq;
	u8	unknown2;
	u16	cache_mem;
	u16	unknown3;
	u8	id[2];
	u16	info;
	struct translation_mode current_tm[4];
	u8	pad[SECTOR_WORDS*4 - 32];
};

#endif /* __PROMISE_4030 */

#endif /* IDE_PROMISE_H */
