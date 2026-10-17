/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_ACPI_APPLE_H_
#define	_ACPI_APPLE_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/backlight/backlight.h>

enum acpi_apple_mode {
	ACPI_APPLE_PLATFORM,		/* WoL + lid (APP000B/APP9876) */
	ACPI_APPLE_BACKLIGHT,		/* panel backlight (APP0002) */
};

/*
 * Apple GMUX (GPU multiplexer) registers.
 * On dual-GPU MacBook Pros, the GMUX owns the backlight PWM.
 * Port offsets are relative to the GMUX resource base, consumed from
 * the APP000B device's _CRS.
 *
 * Three I/O protocols exist:
 *   Classic (PIO)  - pre-Retina Macs: direct inb/outb at base+port.
 *   Indexed        - Retina Macs (2012-2015): write port number to
 *                    GMUX_PORT_READ/WRITE, data at GMUX_PORT_VALUE.
 *   MMIO           - T2 Macs: memory-mapped, select port+direction via
 *                    GMUX_MMIO_PORT_SELECT, trigger with
 *                    GMUX_MMIO_COMMAND_SEND, data at the resource base
 *                    as a big-endian 32-bit value.
 */
#define	GMUX_PORT_VERSION_MAJOR		0x04
#define	GMUX_PORT_VERSION_MINOR		0x05
#define	GMUX_PORT_VERSION_RELEASE	0x06
#define	GMUX_PORT_MAX_BRIGHTNESS	0x70
#define	GMUX_PORT_BRIGHTNESS		0x74
#define	GMUX_BRIGHTNESS_MASK		0x00ffffff

/* A version read returns major, minor and release packed into 32 bits. */
#define	GMUX_VERSION_MAJOR(v)		(((v) >> 24) & 0xff)
#define	GMUX_VERSION_MINOR(v)		(((v) >> 16) & 0xff)
#define	GMUX_VERSION_RELEASE(v)		(((v) >> 8) & 0xff)

/*
 * An absent GMUX floats the bus high, so a read of all ones means the
 * protocol under test is not the one this machine speaks.
 */
#define	GMUX_FLOATING			0xff

/* Handshake polling: microseconds per iteration, and iteration limits. */
#define	GMUX_POLL_DELAY			100
#define	GMUX_INDEXED_RETRIES		100
#define	GMUX_MMIO_RETRIES		200

/* Indexed GMUX registers */
#define	GMUX_PORT_VALUE			0xc2
#define	GMUX_PORT_READ			0xd0
#define	GMUX_PORT_WRITE			0xd4

/* Bit 0 of GMUX_PORT_WRITE: command outstanding, cleared once complete. */
#define	GMUX_INDEXED_BUSY		0x01

/*
 * Indexed-GMUX detection: write a fixed 3-byte unlock sequence to
 * PORT_INDEXED_{MAGIC1,MAGIC2,MAGIC3}, then read the same two ports
 * back and expect them to echo GMUX_INDEXED_SIGNATURE.
 */
#define	GMUX_PORT_INDEXED_MAGIC1	0xcc
#define	GMUX_PORT_INDEXED_MAGIC2	0xcd
#define	GMUX_PORT_INDEXED_MAGIC3	0xce
#define	GMUX_INDEXED_UNLOCK1		0xaa
#define	GMUX_INDEXED_UNLOCK2		0x55
#define	GMUX_INDEXED_UNLOCK3		0x00
#define	GMUX_INDEXED_SIGNATURE		0x55aa

/* MMIO GMUX registers */
#define	GMUX_MMIO_PORT_SELECT		0x0e
#define	GMUX_MMIO_COMMAND_SEND		0x0f
#define	GMUX_MMIO_READ			0x00
#define	GMUX_MMIO_WRITE			0x40

/* Minimum resource length needed to reach GMUX_PORT_BRIGHTNESS+4. */
#define	GMUX_MIN_IO_LEN			(GMUX_PORT_BRIGHTNESS + 4)

#define	GMUX_TYPE_PIO			0
#define	GMUX_TYPE_INDEXED		1
#define	GMUX_TYPE_MMIO			2

struct apple_bl_reg {
	uint32_t	offset;
	uint32_t	mask;
	uint8_t		shift;
};

struct apple_bl_hw {
	const char		*name;
	int			bar;
	vm_size_t		mapsize;
	struct apple_bl_reg	ctrl;
	struct apple_bl_reg	enable;
	uint32_t		enable_val;
	struct apple_bl_reg	max_reg;
	uint32_t		max;
};

struct acpi_apple_softc {
	device_t		dev;
	ACPI_HANDLE		handle;
	enum acpi_apple_mode	mode;

	/* Backlight */
	struct mtx		bl_mtx;		/* protects hw access + cache */
	struct cdev		*bl_cdev;
	const struct apple_bl_hw *bl_hw;
	void			*bl_mmio;	/* GPU BAR MMIO (non-GMUX) */
	uint16_t		bl_gmux_base;	/* GMUX I/O port base, 0=none */
	void			*bl_gmux_mmio;	/* GMUX MMIO base, NULL=none */
	uint8_t			bl_gmux_type;	/* GMUX_TYPE_* */
	uint32_t		bl_max;
	uint32_t		bl_brightness;	/* cached percent 0..100 */
};

/* acpi_apple_platform.c */
int	acpi_apple_platform_attach(struct acpi_apple_softc *sc);
void	acpi_apple_platform_detach(struct acpi_apple_softc *sc);

/* acpi_apple_bl.c */
int	acpi_apple_backlight_attach(struct acpi_apple_softc *sc);
void	acpi_apple_backlight_detach(struct acpi_apple_softc *sc);
void	acpi_apple_backlight_restore(void);
void	acpi_apple_backlight_off(void);
int	acpi_apple_bl_update_status(device_t dev,
	    struct backlight_props *props);
int	acpi_apple_bl_get_status(device_t dev,
	    struct backlight_props *props);
int	acpi_apple_bl_get_info(device_t dev,
	    struct backlight_info *info);

#endif /* _ACPI_APPLE_H_ */
