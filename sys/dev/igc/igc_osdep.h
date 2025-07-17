/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _FREEBSD_OS_H_
#define _FREEBSD_OS_H_

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/iflib.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#define usec_delay(x) DELAY(x)
#define usec_delay_irq(x) usec_delay(x)
#define msec_delay(x) DELAY(1000*(x))
#define msec_delay_irq(x) DELAY(1000*(x))

/* Enable/disable debugging statements in shared code */
#define DBG		0

#define DEBUGOUT(...) \
    do { if (DBG) printf(__VA_ARGS__); } while (0)
#define DEBUGOUT1(...)			DEBUGOUT(__VA_ARGS__)
#define DEBUGOUT2(...)			DEBUGOUT(__VA_ARGS__)
#define DEBUGOUT3(...)			DEBUGOUT(__VA_ARGS__)
#define DEBUGOUT7(...)			DEBUGOUT(__VA_ARGS__)
#define DEBUGFUNC(F)			DEBUGOUT(F "\n")

typedef uint64_t	u64;
typedef uint32_t	u32;
typedef uint16_t	u16;
typedef uint8_t		u8;
typedef int64_t		s64;
typedef int32_t		s32;
typedef int16_t		s16;
typedef int8_t		s8;

#define __le16		u16
#define __le32		u32
#define __le64		u64

struct igc_osdep
{
	bus_space_tag_t    mem_bus_space_tag;
	bus_space_handle_t mem_bus_space_handle;
	bus_space_tag_t    io_bus_space_tag;
	bus_space_handle_t io_bus_space_handle;
	bus_space_tag_t    flash_bus_space_tag;
	bus_space_handle_t flash_bus_space_handle;
	device_t           dev;
	if_ctx_t           ctx;
};

#define IGC_REGISTER(hw, reg) reg

#define IGC_WRITE_FLUSH(a) IGC_READ_REG(a, IGC_STATUS)

/* Read from an absolute offset in the adapter's memory space */
#define IGC_READ_OFFSET(hw, offset) \
    bus_space_read_4(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
    ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, offset)

/* Write to an absolute offset in the adapter's memory space */
#define IGC_WRITE_OFFSET(hw, offset, value) \
    bus_space_write_4(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
    ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, offset, value)

/* Register READ/WRITE macros */

#define IGC_READ_REG(hw, reg) \
    bus_space_read_4(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
        ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, \
        IGC_REGISTER(hw, reg))

#define IGC_WRITE_REG(hw, reg, value) \
    bus_space_write_4(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
        ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, \
        IGC_REGISTER(hw, reg), value)

#define IGC_READ_REG_ARRAY(hw, reg, index) \
    bus_space_read_4(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
        ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, \
        IGC_REGISTER(hw, reg) + ((index)<< 2))

#define IGC_WRITE_REG_ARRAY(hw, reg, index, value) \
    bus_space_write_4(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
        ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, \
        IGC_REGISTER(hw, reg) + ((index)<< 2), value)

#define IGC_READ_REG_ARRAY_DWORD IGC_READ_REG_ARRAY
#define IGC_WRITE_REG_ARRAY_DWORD IGC_WRITE_REG_ARRAY

#define IGC_READ_REG_ARRAY_BYTE(hw, reg, index) \
    bus_space_read_1(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
        ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, \
        IGC_REGISTER(hw, reg) + index)

#define IGC_WRITE_REG_ARRAY_BYTE(hw, reg, index, value) \
    bus_space_write_1(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
        ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, \
        IGC_REGISTER(hw, reg) + index, value)

#define IGC_WRITE_REG_ARRAY_WORD(hw, reg, index, value) \
    bus_space_write_2(((struct igc_osdep *)(hw)->back)->mem_bus_space_tag, \
        ((struct igc_osdep *)(hw)->back)->mem_bus_space_handle, \
        IGC_REGISTER(hw, reg) + (index << 1), value)

#endif  /* _FREEBSD_OS_H_ */
