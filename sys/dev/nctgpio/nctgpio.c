/*-
 * Copyright (c) 2016 Daniel Wyatt <Daniel.Wyatt@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Nuvoton GPIO driver.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>

#include <sys/module.h>
#include <sys/gpio.h>

#include <machine/bus.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/superio/superio.h>

#include "gpio_if.h"

#define NCT_PPOD_LDN 0xf /* LDN used to select Push-Pull/Open-Drain */

/* Direct access through GPIO register table */
#define	NCT_IO_GSR			0 /* Group Select */
#define	NCT_IO_IOR			1 /* I/O */
#define	NCT_IO_DAT			2 /* Data */
#define	NCT_IO_INV			3 /* Inversion */
#define	NCT_IO_DST          4 /* Status */

#define NCT_MAX_GROUP   9
#define NCT_MAX_PIN     75

#define NCT_PIN_IS_VALID(_sc, _p)   ((_p) < (_sc)->npins)
#define NCT_PIN_GROUP(_sc, _p)      ((_sc)->pinmap[(_p)].group)
#define NCT_PIN_GRPNUM(_sc, _p)     ((_sc)->pinmap[(_p)].grpnum)
#define NCT_PIN_BIT(_sc, _p)        ((_sc)->pinmap[(_p)].bit)
#define NCT_PIN_BITMASK(_p)         (1 << ((_p) & 7))

#define NCT_GPIO_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT | \
	GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL | \
	GPIO_PIN_INVIN | GPIO_PIN_INVOUT)

#define NCT_PREFER_INDIRECT_CHANNEL 2

#define NCT_VERBOSE_PRINTF(dev, ...)            \
	do {                                        \
		if (__predict_false(bootverbose))       \
			device_printf(dev, __VA_ARGS__);    \
	} while (0)

/*
 * Note that the values are important.
 * They match actual register offsets.
 */
typedef enum {
	REG_IOR = 0,
	REG_DAT = 1,
	REG_INV = 2,
} reg_t;

struct nct_gpio_group {
 	uint32_t    caps;
	uint8_t     enable_ldn;
	uint8_t     enable_reg;
	uint8_t     enable_mask;
	uint8_t     data_ldn;
	uint8_t     iobase;
	uint8_t     ppod_reg; /* Push-Pull/Open-Drain */
	uint8_t     grpnum;
	uint8_t     pinbits[8];
	uint8_t     npins;
};

struct nct_softc {
	device_t			dev;
	device_t			busdev;
	struct mtx			mtx;
	struct resource			*iores;
	int				iorid;
	int				curgrp;
	struct {
		uint8_t ior[NCT_MAX_GROUP + 1];       /* direction, 1: input 0: output */
		uint8_t out[NCT_MAX_GROUP + 1];       /* output value */
		uint8_t out_known[NCT_MAX_GROUP + 1]; /* whether out is valid */
		uint8_t inv[NCT_MAX_GROUP + 1];       /* inversion, 1: inverted */
	} cache;
	struct gpio_pin				pins[NCT_MAX_PIN + 1];
	struct nct_device			*nctdevp;
	int							npins; /* Total number of pins */

	/* Lookup tables */
	struct {
		struct nct_gpio_group *group;
		uint8_t                grpnum;
		uint8_t                bit;
	} pinmap[NCT_MAX_PIN+1];
	struct nct_gpio_group *grpmap[NCT_MAX_GROUP+1];
};

#define GPIO_LOCK_INIT(_sc)	mtx_init(&(_sc)->mtx,		\
		device_get_nameunit(dev), NULL, MTX_DEF)
#define GPIO_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->mtx)
#define GPIO_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define GPIO_UNLOCK(_sc)	mtx_unlock(&(_sc)->mtx)
#define GPIO_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)
#define GPIO_ASSERT_UNLOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_NOTOWNED)

#define GET_BIT(v, b)	(((v) >> (b)) & 1)

/*
 * For most devices there are several GPIO devices, we attach only to one of
 * them and use the rest without attaching.
 */
struct nct_device {
	uint16_t                  devid;
	int                       extid;
	const char               *descr;
	int                       ngroups;
	struct nct_gpio_group     groups[NCT_MAX_GROUP + 1];
} nct_devices[] = {
	{
		.devid   = 0xa025,
		.descr   = "GPIO on Winbond 83627DHG IC ver. 5",
		.ngroups = 5,
		.groups  = {
			{
				.grpnum      = 2,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe0, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe3,
			},
			{
				.grpnum      = 3,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf0,
			},
			{
				.grpnum      = 4,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x04,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf4,
			},
			{
				.grpnum      = 5,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x08,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 6,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf4,
			},
		},
	},
	{
		.devid   = 0x1061,
		.descr   = "GPIO on Nuvoton NCT5104D",
		.ngroups = 2,
		.groups  = {
			{
				.grpnum      = 0,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe0,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 1,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe4,
			},
		},
	},
	{
		.devid   = 0xc452, /* FIXME Conflict with Nuvoton NCT6106D. See NetBSD's nct_match. */
		.descr   = "GPIO on Nuvoton NCT5104D (PC-Engines APU)",
		.ngroups = 2,
		.groups  = {
			{
				.grpnum      = 0,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe0,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 1,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe4,
			},
		},
	},
	{
		.devid   = 0xc453,
		.descr   = "GPIO on Nuvoton NCT5104D (PC-Engines APU3)",
		.ngroups = 2,
		.groups  = {
			{
				.grpnum      = 0,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe0,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 1,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe4,
			},
		},
	},
	{
		.devid  = 0xd42a,
		.extid  = 1,
		.descr  = "GPIO on Nuvoton NCT6796D-E",
		.ngroups = 10,
		.groups  = {
			{
				.grpnum      = 0,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x08,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x08,
				.ppod_reg    = 0xe0, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 1,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x08,
				.enable_reg  = 0x30,
				.enable_mask = 0x80,
				.data_ldn    = 0x08,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf0,
			},
			{
				.grpnum      = 2,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 3,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 7,
				.iobase      = 0xe4,
			},
			{
				.grpnum      = 4,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x04,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf0, /* FIXME Page 344 say "F0~F2, E8",
										not "F0~F3". */
			},
			{
				.grpnum      = 5,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x08,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf4,
			},
			{
				.grpnum      = 6,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf4,
			},
			{
				.grpnum      = 7,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 8,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x04,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe4,
			},
			{
				.grpnum      = 9,
				.pinbits     = { 0, 1, 2, 3 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x08,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 4,
				.iobase      = 0xe8,
			},
		},
	},
	{
		.devid   = 0xd42a,
		.extid   = 2,
		.descr   = "GPIO on Nuvoton NCT5585D",
		.ngroups = 6,
		.groups  = {
			{
				.grpnum      = 2,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 7,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 3,
				.pinbits     = { 1, 2, 3 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe2,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 3,
				.iobase      = 0xe4,
			},
			{
				.grpnum      = 5,
				.pinbits     = { 0, 2, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x08,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe4,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 4,
				.iobase      = 0xf4,
			},
			{
				.grpnum      = 7,
				.pinbits     = { 4 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe6,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 1,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 8,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x04,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe7,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe4,
			},
			{
				.grpnum      = 9,
				.pinbits     = { 0, 2 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x08,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xea,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 2,
				.iobase      = 0xe8,
			},
		},
	},
	{
		.devid   = 0xc562,
		.descr   = "GPIO on Nuvoton NCT6779D",
		.ngroups = 9,
		.groups  = {
			{
				.grpnum      = 0,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x08,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x08,
				.ppod_reg    = 0xe0, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 1,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x08,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf0,
			},
			{
				.grpnum      = 2,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 3,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 7,
				.iobase      = 0xe4,
			},
			{
				.grpnum      = 4,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x04,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf0,
			},
			{
				.grpnum      = 5,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x08,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf4,
			},
			{
				.grpnum      = 6,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf4,
			},
			{
				.grpnum      = 7,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 7,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 8,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x04,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1, /* FIXME Need to check for this group. */
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe4,
			},
		},
	},
	{
		.devid   = 0xd282,
		.descr   = "GPIO on Nuvoton NCT6112D/NCT6114D/NCT6116D",
		.ngroups = 9,
		.groups  = {
			{
				.grpnum      = 0,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe0,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe0,
			},
			{
				.grpnum      = 1,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x02,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe4,
			},
			{
				.grpnum      = 2,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x04,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xe8,
			},
			{
				.grpnum      = 3,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x08,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xec,
			},
			{
				.grpnum      = 4,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x10,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf0,
			},
			{
				.grpnum      = 5,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x20,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf4,
			},
			{
				.grpnum      = 6,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x40,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf8,
			},
			{
				.grpnum      = 7,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x07,
				.enable_reg  = 0x30,
				.enable_mask = 0x80,
				.data_ldn    = 0x07,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xfc,
			},
			{
				.grpnum      = 8,
				.pinbits     = { 0, 1, 2, 3, 4, 5, 6, 7 },
				.enable_ldn  = 0x09,
				.enable_reg  = 0x30,
				.enable_mask = 0x01,
				.data_ldn    = 0x09,
				.ppod_reg    = 0xe1,
				.caps        = NCT_GPIO_CAPS,
				.npins       = 8,
				.iobase      = 0xf0,
			},
		},
	},
};

static const char *
io2str(uint8_t ioport)
{
	switch (ioport) {
	case NCT_IO_GSR: return ("grpsel");
	case NCT_IO_IOR: return ("io");
	case NCT_IO_DAT: return ("data");
	case NCT_IO_INV: return ("inv");
	case NCT_IO_DST: return ("status");
	default:         return ("?");
	}
}

static void
nct_io_set_group(struct nct_softc *sc, uint8_t grpnum)
{
	GPIO_ASSERT_LOCKED(sc);

	if (grpnum == sc->curgrp)
		return;

	NCT_VERBOSE_PRINTF(sc->dev, "write %s 0x%x ioport %d\n",
		io2str(NCT_IO_GSR), grpnum, NCT_IO_GSR);
	bus_write_1(sc->iores, NCT_IO_GSR, grpnum);
	sc->curgrp = grpnum;
}

static uint8_t
nct_io_read(struct nct_softc *sc, uint8_t grpnum, uint8_t reg)
{
	uint8_t val;

	nct_io_set_group(sc, grpnum);

	val = bus_read_1(sc->iores, reg);
	NCT_VERBOSE_PRINTF(sc->dev, "read %s 0x%x ioport %d\n",
		io2str(reg), val, reg);
	return (val);
}

static void
nct_io_write(struct nct_softc *sc, uint8_t grpnum, uint8_t reg, uint8_t val)
{
	nct_io_set_group(sc, grpnum);

	NCT_VERBOSE_PRINTF(sc->dev, "write %s 0x%x ioport %d\n",
		io2str(reg), val, reg);
	bus_write_1(sc->iores, reg, val);
}

static uint8_t
nct_get_ioreg(struct nct_softc *sc, reg_t reg, uint8_t grpnum)
{
	uint8_t iobase;

	if (sc->iores != NULL)
		iobase = NCT_IO_IOR;
	else
		iobase = sc->grpmap[grpnum]->iobase;
	return (iobase + reg);
}

static const char *
reg2str(reg_t reg)
{
	switch (reg) {
	case REG_IOR: return ("io");
	case REG_DAT: return ("data");
	case REG_INV: return ("inv");
	default:      return ("?");
	}
}

static uint8_t
nct_read_reg(struct nct_softc *sc, reg_t reg, uint8_t grpnum)
{
	struct nct_gpio_group *gp;
	uint8_t                ioreg;
	uint8_t                val;

	ioreg = nct_get_ioreg(sc, reg, grpnum);

	if (sc->iores != NULL)
		return (nct_io_read(sc, grpnum, ioreg));

	gp  = sc->grpmap[grpnum];
	val = superio_ldn_read(sc->dev, gp->data_ldn, ioreg);
	NCT_VERBOSE_PRINTF(sc->dev, "read %s 0x%x from group GPIO%u ioreg 0x%x\n",
		reg2str(reg), val, grpnum, ioreg);
	return (val);
}

static int
nct_get_pin_cache(struct nct_softc *sc, uint32_t pin_num, uint8_t *cache)
{
	uint8_t bit;
	uint8_t group;
	uint8_t val;

	KASSERT(NCT_PIN_IS_VALID(sc, pin_num), ("%s: invalid pin number %d",
	    __func__, pin_num));

	group = NCT_PIN_GRPNUM(sc, pin_num);
	bit   = NCT_PIN_BIT(sc, pin_num);
	val   = cache[group];
	return (GET_BIT(val, bit));
}

static void
nct_write_reg(struct nct_softc *sc, reg_t reg, uint8_t grpnum, uint8_t val)
{
	struct nct_gpio_group *gp;
	uint8_t                ioreg;

	ioreg = nct_get_ioreg(sc, reg, grpnum);

	if (sc->iores != NULL) {
		nct_io_write(sc, grpnum, ioreg, val);
		return;
	}

	gp = sc->grpmap[grpnum];
	superio_ldn_write(sc->dev, gp->data_ldn, ioreg, val);

	NCT_VERBOSE_PRINTF(sc->dev, "write %s 0x%x to group GPIO%u ioreg 0x%x\n",
		reg2str(reg), val, grpnum, ioreg);
}

static void
nct_set_pin_reg(struct nct_softc *sc, reg_t reg, uint32_t pin_num, bool val)
{
	uint8_t *cache;
	uint8_t bit;
	uint8_t bitval;
	uint8_t group;
	uint8_t mask;

	KASSERT(NCT_PIN_IS_VALID(sc, pin_num),
	    ("%s: invalid pin number %d", __func__, pin_num));
	KASSERT(reg == REG_IOR || reg == REG_INV,
	    ("%s: unsupported register %d", __func__, reg));

	group  = NCT_PIN_GRPNUM(sc, pin_num);
	bit    = NCT_PIN_BIT(sc, pin_num);
	mask   = (uint8_t)1 << bit;
	bitval = (uint8_t)val << bit;

	if (reg == REG_IOR)
		cache = &sc->cache.ior[group];
	else
		cache = &sc->cache.inv[group];
	if ((*cache & mask) == bitval)
		return;
	*cache &= ~mask;
	*cache |= bitval;
	nct_write_reg(sc, reg, group, *cache);
}

/*
 * Set a pin to input (val is true) or output (val is false) mode.
 */
static void
nct_set_pin_input(struct nct_softc *sc, uint32_t pin_num, bool val)
{
	nct_set_pin_reg(sc, REG_IOR, pin_num, val);
}

/*
 * Check whether a pin is configured as an input.
 */
static bool
nct_pin_is_input(struct nct_softc *sc, uint32_t pin_num)
{
	return (nct_get_pin_cache(sc, pin_num, sc->cache.ior));
}

/*
 * Set a pin to inverted (val is true) or normal (val is false) mode.
 */
static void
nct_set_pin_inverted(struct nct_softc *sc, uint32_t pin_num, bool val)
{
	nct_set_pin_reg(sc, REG_INV, pin_num, val);
}

static bool
nct_pin_is_inverted(struct nct_softc *sc, uint32_t pin_num)
{
	return (nct_get_pin_cache(sc, pin_num, sc->cache.inv));
}

/*
 * Write a value to an output pin.
 * NB: the hardware remembers last output value across switching from
 * output mode to input mode and back.
 * Writes to a pin in input mode are not allowed here as they cannot
 * have any effect and would corrupt the output value cache.
 */
static void
nct_write_pin(struct nct_softc *sc, uint32_t pin_num, bool val)
{
	uint8_t bit;
	uint8_t group;

	KASSERT(!nct_pin_is_input(sc, pin_num), ("attempt to write input pin"));
	group = NCT_PIN_GRPNUM(sc, pin_num);
	bit   = NCT_PIN_BIT(sc, pin_num);

	if (GET_BIT(sc->cache.out_known[group], bit) &&
	    GET_BIT(sc->cache.out[group], bit) == val) {
		/* The pin is already in requested state. */
		return;
	}
	sc->cache.out_known[group] |= 1 << bit;
	if (val)
		sc->cache.out[group] |= 1 << bit;
	else
		sc->cache.out[group] &= ~(1 << bit);
	nct_write_reg(sc, REG_DAT, group, sc->cache.out[group]);
}

static bool
nct_get_pin_reg(struct nct_softc *sc, reg_t reg, uint32_t pin_num)
{
	uint8_t            bit;
	uint8_t            group;
	uint8_t            val;
	bool               b;

	KASSERT(NCT_PIN_IS_VALID(sc, pin_num), ("%s: invalid pin number %d",
			__func__, pin_num));

	group = NCT_PIN_GRPNUM(sc, pin_num);
	bit   = NCT_PIN_BIT(sc, pin_num);
	val   = nct_read_reg(sc, reg, group);
	b     = GET_BIT(val, bit);

	if (__predict_false(bootverbose)) {
		if (nct_pin_is_input(sc, pin_num))
			NCT_VERBOSE_PRINTF(sc->dev, "read %d from input pin %u<GPIO%u%u>\n",
				b, pin_num, group, bit);
		else
			NCT_VERBOSE_PRINTF(sc->dev,
				"read %d from output pin %u<GPIO%u%u>, cache miss\n",
				b, pin_num, group, bit);
	}

	return (b);
}

/*
 * NB: state of an input pin cannot be cached, of course.
 * For an output we can either take the value from the cache if it's valid
 * or read the state from the hadrware and cache it.
 */
static bool
nct_read_pin(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t bit;
	uint8_t group;
	bool    val;

	if (nct_pin_is_input(sc, pin_num)) {
		return (nct_get_pin_reg(sc, REG_DAT, pin_num));
	}

	group = NCT_PIN_GRPNUM(sc, pin_num);
	bit   = NCT_PIN_BIT(sc, pin_num);

	if (GET_BIT(sc->cache.out_known[group], bit)) {
		val = GET_BIT(sc->cache.out[group], bit);

		NCT_VERBOSE_PRINTF(sc->dev,
			"read %d from output pin %u<GPIO%u%u>, cache hit\n",
			val, pin_num, group, bit);

		return (val);
	}

	val = nct_get_pin_reg(sc, REG_DAT, pin_num);
	sc->cache.out_known[group] |= 1 << bit;
	if (val)
		sc->cache.out[group] |= 1 << bit;
	else
		sc->cache.out[group] &= ~(1 << bit);
	return (val);
}

/* FIXME Incorret for NCT5585D and probably other chips. */
static uint8_t
nct_ppod_reg(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t group = NCT_PIN_GRPNUM(sc, pin_num);

	return (sc->grpmap[group]->ppod_reg);
}

/*
 * NB: PP/OD can be configured only via configuration registers.
 * Also, the registers are in a different logical device.
 * So, this is a special case.  No caching too.
 */
static void
nct_set_pin_opendrain(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t reg;
	uint8_t outcfg;

	reg = nct_ppod_reg(sc, pin_num);
	outcfg = superio_ldn_read(sc->dev, NCT_PPOD_LDN, reg);
	outcfg |= NCT_PIN_BITMASK(pin_num);
	superio_ldn_write(sc->dev, 0xf, reg, outcfg);
}

static void
nct_set_pin_pushpull(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t reg;
	uint8_t outcfg;

	reg = nct_ppod_reg(sc, pin_num);
	outcfg = superio_ldn_read(sc->dev, NCT_PPOD_LDN, reg);
	outcfg &= ~NCT_PIN_BITMASK(pin_num);
	superio_ldn_write(sc->dev, 0xf, reg, outcfg);
}

static bool
nct_pin_is_opendrain(struct nct_softc *sc, uint32_t pin_num)
{
	uint8_t reg;
	uint8_t outcfg;

	reg = nct_ppod_reg(sc, pin_num);
	outcfg = superio_ldn_read(sc->dev, NCT_PPOD_LDN, reg);
	return (outcfg & NCT_PIN_BITMASK(pin_num));
}

static struct nct_device *
nct_lookup_device(device_t dev)
{
	struct nct_device *nctdevp;
	uint16_t           devid;
	int                i, extid;

	devid = superio_devid(dev);
	extid = superio_extid(dev);
	for (i = 0, nctdevp = nct_devices; i < nitems(nct_devices); i++, nctdevp++) {
		if (devid == nctdevp->devid && nctdevp->extid == extid)
			return (nctdevp);
	}
	return (NULL);
}

static int
nct_probe(device_t dev)
{
	struct nct_device *nctdevp;
	uint8_t            ldn;

	ldn = superio_get_ldn(dev);

	if (superio_vendor(dev) != SUPERIO_VENDOR_NUVOTON) {
		NCT_VERBOSE_PRINTF(dev, "ldn 0x%x not a Nuvoton device\n", ldn);
		return (ENXIO);
	}
	if (superio_get_type(dev) != SUPERIO_DEV_GPIO) {
		NCT_VERBOSE_PRINTF(dev, "ldn 0x%x not a GPIO device\n", ldn);
		return (ENXIO);
	}

	nctdevp = nct_lookup_device(dev);
	if (nctdevp == NULL) {
		NCT_VERBOSE_PRINTF(dev, "ldn 0x%x not supported\n", ldn);
		return (ENXIO);
	}
	device_set_desc(dev, nctdevp->descr);
	return (BUS_PROBE_DEFAULT);
}

static int
nct_attach(device_t dev)
{
	struct nct_softc *sc;
	struct nct_gpio_group *gp;
	uint32_t pin_num;
	uint8_t v;
	int flags, i, g;

	sc          = device_get_softc(dev);
	sc->dev     = dev;
	sc->nctdevp = nct_lookup_device(dev);

	flags = 0;
	(void)resource_int_value(device_get_name(dev), device_get_unit(dev), "flags", &flags);

	if ((flags & NCT_PREFER_INDIRECT_CHANNEL) == 0) {
		uint16_t iobase;
		device_t dev_8;

		/*
		 * As strange as it may seem, I/O port base is configured in the
		 * Logical Device 8 which is primarily used for WDT, but also plays
		 * a role in GPIO configuration.
		 */
		iobase = 0;
		dev_8 = superio_find_dev(device_get_parent(dev), SUPERIO_DEV_WDT, 8);
		if (dev_8 != NULL)
			iobase = superio_get_iobase(dev_8);
		if (iobase != 0 && iobase != 0xffff) {
			int err;

			NCT_VERBOSE_PRINTF(dev, "iobase %#x\n", iobase);
			sc->curgrp = -1;
			sc->iorid = 0;
			err = bus_set_resource(dev, SYS_RES_IOPORT, sc->iorid,
				iobase, 7); /* FIXME NCT6796D-E have 8 registers according to table 18.3. */
			if (err == 0) {
				sc->iores = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
					&sc->iorid, RF_ACTIVE);
				if (sc->iores == NULL) {
					device_printf(dev, "can't map i/o space, "
						"iobase=%#x\n", iobase);
				}
			} else {
				device_printf(dev,
					"failed to set io port resource at %#x\n", iobase);
			}
		}
	}
	NCT_VERBOSE_PRINTF(dev, "iores %p %s channel\n",
		sc->iores, (sc->iores ? "direct" : "indirect"));

	/* Enable GPIO groups */
	for (g = 0, gp = sc->nctdevp->groups; g < sc->nctdevp->ngroups; g++, gp++) {
		NCT_VERBOSE_PRINTF(dev,
			"GPIO%d: %d pins, enable with mask 0x%x via ldn 0x%x reg 0x%x\n",
			gp->grpnum, gp->npins, gp->enable_mask, gp->enable_ldn,
			gp->enable_reg);
		v = superio_ldn_read(dev, gp->enable_ldn, gp->enable_reg);
		v |= gp->enable_mask;
		superio_ldn_write(dev, gp->enable_ldn, gp->enable_reg, v);
	}

	GPIO_LOCK_INIT(sc);
	GPIO_LOCK(sc);

	pin_num   = 0;
	sc->npins = 0;
	for (g = 0, gp = sc->nctdevp->groups; g < sc->nctdevp->ngroups; g++, gp++) {

		sc->grpmap[gp->grpnum] = gp;

		/*
		 * Caching input values is meaningless as an input can be changed at any
		 * time by an external agent.  But outputs are controlled by this
		 * driver, so it can cache their state.  Also, the hardware remembers
		 * the output state of a pin when the pin is switched to input mode and
		 * then back to output mode.  So, the cache stays valid.
		 * The only problem is with pins that are in input mode at the attach
		 * time.  For them the output state is not known until it is set by the
		 * driver for the first time.
		 * 'out' and 'out_known' bits form a tri-state output cache:
		 * |-----+-----------+---------|
		 * | out | out_known | cache   |
		 * |-----+-----------+---------|
		 * |   X |         0 | invalid |
		 * |   0 |         1 |       0 |
		 * |   1 |         1 |       1 |
		 * |-----+-----------+---------|
		 */
		sc->cache.inv[gp->grpnum]       = nct_read_reg(sc, REG_INV, gp->grpnum);
		sc->cache.ior[gp->grpnum]       = nct_read_reg(sc, REG_IOR, gp->grpnum);
		sc->cache.out[gp->grpnum]       = nct_read_reg(sc, REG_DAT, gp->grpnum);
		sc->cache.out_known[gp->grpnum] = ~sc->cache.ior[gp->grpnum];

		sc->npins += gp->npins;
		for (i = 0; i < gp->npins; i++, pin_num++) {
			struct gpio_pin *pin;

			sc->pinmap[pin_num].group  = gp;
			sc->pinmap[pin_num].grpnum = gp->grpnum;
			sc->pinmap[pin_num].bit    = gp->pinbits[i];

			pin           = &sc->pins[pin_num];
			pin->gp_pin   = pin_num;
			pin->gp_caps  = gp->caps;
			pin->gp_flags = 0;

			snprintf(pin->gp_name, GPIOMAXNAME, "GPIO%u%u",
				gp->grpnum, gp->pinbits[i]);

			if (nct_pin_is_input(sc, pin_num))
				pin->gp_flags |= GPIO_PIN_INPUT;
			else
				pin->gp_flags |= GPIO_PIN_OUTPUT;

			if (nct_pin_is_opendrain(sc, pin_num))
				pin->gp_flags |= GPIO_PIN_OPENDRAIN;
			else
				pin->gp_flags |= GPIO_PIN_PUSHPULL;

			if (nct_pin_is_inverted(sc, pin_num))
				pin->gp_flags |= (GPIO_PIN_INVIN | GPIO_PIN_INVOUT);
		}
	}
	NCT_VERBOSE_PRINTF(dev, "%d pins available\n", sc->npins);

	GPIO_UNLOCK(sc);

	sc->busdev = gpiobus_add_bus(dev);
	if (sc->busdev == NULL) {
		device_printf(dev, "failed to attach to gpiobus\n");
		GPIO_LOCK_DESTROY(sc);
		return (ENXIO);
	}

	bus_attach_children(dev);
	return (0);
}

static int
nct_detach(device_t dev)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);
	gpiobus_detach_bus(dev);

	if (sc->iores != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->iorid, sc->iores);
	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK_DESTROY(sc);

	return (0);
}

static device_t
nct_gpio_get_bus(device_t dev)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

static int
nct_gpio_pin_max(device_t dev, int *maxpin)
{
	struct nct_softc *sc;

	sc      = device_get_softc(dev);
	*maxpin = sc->npins - 1;
	return (0);
}

static int
nct_gpio_pin_set(device_t dev, uint32_t pin_num, uint32_t pin_value)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);

	if (!NCT_PIN_IS_VALID(sc, pin_num))
		return (EINVAL);

	GPIO_LOCK(sc);
	if ((sc->pins[pin_num].gp_flags & GPIO_PIN_OUTPUT) == 0) {
		GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	nct_write_pin(sc, pin_num, pin_value);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_get(device_t dev, uint32_t pin_num, uint32_t *pin_value)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);

	if (!NCT_PIN_IS_VALID(sc, pin_num))
		return (EINVAL);

	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	*pin_value = nct_read_pin(sc, pin_num);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_toggle(device_t dev, uint32_t pin_num)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);

	if (!NCT_PIN_IS_VALID(sc, pin_num))
		return (EINVAL);

	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	if ((sc->pins[pin_num].gp_flags & GPIO_PIN_OUTPUT) == 0) {
		GPIO_UNLOCK(sc);
		return (EINVAL);
	}
	if (nct_read_pin(sc, pin_num))
		nct_write_pin(sc, pin_num, 0);
	else
		nct_write_pin(sc, pin_num, 1);

	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_getcaps(device_t dev, uint32_t pin_num, uint32_t *caps)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);

	if (!NCT_PIN_IS_VALID(sc, pin_num))
		return (EINVAL);

	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	*caps = sc->pins[pin_num].gp_caps;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_getflags(device_t dev, uint32_t pin_num, uint32_t *flags)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);

	if (!NCT_PIN_IS_VALID(sc, pin_num))
		return (EINVAL);

	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	*flags = sc->pins[pin_num].gp_flags;
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_getname(device_t dev, uint32_t pin_num, char *name)
{
	struct nct_softc *sc;

	sc = device_get_softc(dev);

	if (!NCT_PIN_IS_VALID(sc, pin_num))
		return (EINVAL);

	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);
	memcpy(name, sc->pins[pin_num].gp_name, GPIOMAXNAME);
	GPIO_UNLOCK(sc);

	return (0);
}

static int
nct_gpio_pin_setflags(device_t dev, uint32_t pin_num, uint32_t flags)
{
	struct nct_softc *sc;
	struct gpio_pin *pin;

	sc = device_get_softc(dev);

	if (!NCT_PIN_IS_VALID(sc, pin_num))
		return (EINVAL);

	pin = &sc->pins[pin_num];
	if ((flags & pin->gp_caps) != flags)
		return (EINVAL);

	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) ==
		(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) {
			return (EINVAL);
	}
	if ((flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) ==
		(GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) {
			return (EINVAL);
	}
	if ((flags & (GPIO_PIN_INVIN | GPIO_PIN_INVOUT)) ==
		(GPIO_PIN_INVIN | GPIO_PIN_INVOUT)) {
			return (EINVAL);
	}

	GPIO_ASSERT_UNLOCKED(sc);
	GPIO_LOCK(sc);

	/* input or output */
	if ((flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)) != 0) {
		nct_set_pin_input(sc, pin_num, (flags & GPIO_PIN_INPUT) != 0);
		pin->gp_flags &= ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT);
		pin->gp_flags |= flags & (GPIO_PIN_INPUT | GPIO_PIN_OUTPUT);
	}

	/* invert */
	if ((flags & (GPIO_PIN_INVIN | GPIO_PIN_INVOUT)) != 0) {
		nct_set_pin_inverted(sc, pin_num, 1);
		pin->gp_flags |= (GPIO_PIN_INVIN | GPIO_PIN_INVOUT);
	} else {
		nct_set_pin_inverted(sc, pin_num, 0);
		pin->gp_flags &= ~(GPIO_PIN_INVIN | GPIO_PIN_INVOUT);
	}

	/* Open drain or push pull */
	if ((flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL)) != 0) {
		if (flags & GPIO_PIN_OPENDRAIN)
			nct_set_pin_opendrain(sc, pin_num);
		else
			nct_set_pin_pushpull(sc, pin_num);
		pin->gp_flags &= ~(GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL);
		pin->gp_flags |=
		    flags & (GPIO_PIN_OPENDRAIN | GPIO_PIN_PUSHPULL);
	}
	GPIO_UNLOCK(sc);

	return (0);
}

static device_method_t nct_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nct_probe),
	DEVMETHOD(device_attach,	nct_attach),
	DEVMETHOD(device_detach,	nct_detach),

	/* GPIO */
	DEVMETHOD(gpio_get_bus,		nct_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		nct_gpio_pin_max),
	DEVMETHOD(gpio_pin_get,		nct_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		nct_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	nct_gpio_pin_toggle),
	DEVMETHOD(gpio_pin_getname,	nct_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps,	nct_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags,	nct_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	nct_gpio_pin_setflags),

	DEVMETHOD_END
};

static driver_t nct_driver = {
	"gpio",
	nct_methods,
	sizeof(struct nct_softc)
};

DRIVER_MODULE(nctgpio, superio, nct_driver, NULL, NULL);
MODULE_DEPEND(nctgpio, gpiobus, 1, 1, 1);
MODULE_DEPEND(nctgpio, superio, 1, 1, 1);
MODULE_VERSION(nctgpio, 1);
