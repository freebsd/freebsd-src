/*
 *  linux/drivers/ide/ide_modes.h
 *
 *  Copyright (C) 1996  Linus Torvalds, Igor Abramov, and Mark Lord
 */

#ifndef _IDE_MODES_H
#define _IDE_MODES_H

#include <linux/config.h>

/*
 * Shared data/functions for determining best PIO mode for an IDE drive.
 * Most of this stuff originally lived in cmd640.c, and changes to the
 * ide_pio_blacklist[] table should be made with EXTREME CAUTION to avoid
 * breaking the fragile cmd640.c support.
 */

/*
 * Standard (generic) timings for PIO modes, from ATA2 specification.
 * These timings are for access to the IDE data port register *only*.
 * Some drives may specify a mode, while also specifying a different
 * value for cycle_time (from drive identification data).
 */
typedef struct ide_pio_timings_s {
	int	setup_time;	/* Address setup (ns) minimum */
	int	active_time;	/* Active pulse (ns) minimum */
	int	cycle_time;	/* Cycle time (ns) minimum = (setup + active + recovery) */
} ide_pio_timings_t;

typedef struct ide_pio_data_s {
	u8 pio_mode;
	u8 use_iordy;
	u8 overridden;
	u8 blacklisted;
	unsigned int cycle_time;
} ide_pio_data_t;
	
u8 ide_get_best_pio_mode (ide_drive_t *drive, u8 mode_wanted, u8 max_mode, ide_pio_data_t *d);
extern const ide_pio_timings_t ide_pio_timings[6];
#endif /* _IDE_MODES_H */
