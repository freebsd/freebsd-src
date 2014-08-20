/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_bytecode_bytecode_h_
#define css_bytecode_bytecode_h_

#include <inttypes.h>
#include <stdio.h>

#include <libcss/types.h>
#include <libcss/properties.h>

typedef uint32_t css_code_t; 

typedef enum css_properties_e opcode_t;

enum flag {
	FLAG_IMPORTANT			= (1<<0),
	FLAG_INHERIT			= (1<<1)
};

typedef enum unit {
	UNIT_PX   = 0,
	UNIT_EX   = 1,
	UNIT_EM   = 2,
	UNIT_IN   = 3,
	UNIT_CM   = 4,
	UNIT_MM   = 5,
	UNIT_PT   = 6,
	UNIT_PC   = 7,

	UNIT_PCT  = (1 << 8),

	UNIT_ANGLE = (1 << 9),
	UNIT_DEG  = (1 << 9) + 0,
	UNIT_GRAD = (1 << 9) + 1,
	UNIT_RAD  = (1 << 9) + 2,

	UNIT_TIME = (1 << 10),
	UNIT_MS   = (1 << 10) + 0,
	UNIT_S    = (1 << 10) + 1,

	UNIT_FREQ = (1 << 11),
	UNIT_HZ   = (1 << 11) + 0,
	UNIT_KHZ  = (1 << 11) + 1
} unit;

typedef uint32_t colour;

typedef enum shape {
	SHAPE_RECT = 0
} shape;

static inline css_code_t buildOPV(opcode_t opcode, uint8_t flags, uint16_t value)
{
	return (opcode & 0x3ff) | (flags << 10) | ((value & 0x3fff) << 18);
}

static inline opcode_t getOpcode(css_code_t OPV)
{
	return (OPV & 0x3ff);
}

static inline uint8_t getFlags(css_code_t OPV)
{
	return ((OPV >> 10) & 0xff);
}

static inline uint16_t getValue(css_code_t OPV)
{
	return (OPV >> 18);
}

static inline bool isImportant(css_code_t OPV)
{
	return getFlags(OPV) & 0x1;
}

static inline bool isInherit(css_code_t OPV)
{
	return getFlags(OPV) & 0x2;
}

#endif



