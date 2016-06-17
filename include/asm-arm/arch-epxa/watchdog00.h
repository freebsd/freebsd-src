#ifndef __WATCHDOG_00_H
#define __WATCHDOG_00_H

/*
 * Register definitions for the watchdog
 */

/*
 * Copyright (c) Altera Corporation 2000.
 * All rights reserved.
 */

#define WDOG_CR(base_addr) (WATCHDOG00_TYPE (base_addr  ))
#define WDOG_CR_LK_MSK (0x1)
#define WDOG_CR_LK_OFST (0)
#define WDOG_CR_LK_ENABLE (0x1)
#define WDOG_CR_TRIGGER_MSK (0x3FFFFFF0)
#define WDOG_CR_TRIGGER_OFST (4)

#define WDOG_COUNT(base_addr) (WATCHDOG00_TYPE (base_addr  + 0x4 ))
#define WDOG_COUNT_MSK (0x3FFFFFFF)

#define WDOG_RELOAD(base_addr) (WATCHDOG00_TYPE (base_addr  + 0x8 ))
#define WDOG_RELOAD_MAGIC_1 (0xA5A5A5A5)
#define WDOG_RELOAD_MAGIC_2 (0x5A5A5A5A)

#endif /* __WATCHDOG_00_H */
