/*-
 * This file is in the public domain.
 */
/* $FreeBSD$ */

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

#include <x86/bus.h>

/*
 * The functions:
 *    bus_space_read_8
 *    bus_space_read_region_8
 *    bus_space_write_8
 *    bus_space_write_multi_8
 *    bus_space_write_region_8
 *    bus_space_set_multi_8
 *    bus_space_set_region_8
 *    bus_space_copy_region_8
 *    bus_space_read_multi_8
 * are unimplemented for i386 because there is no way to do a 64-bit move in
 * this architecture. It is possible to do two 32-bit moves, but this is
 * not atomic and may have hardware dependencies that should be fully
 * understood.
 */

#endif /*_MACHINE_BUS_H_*/
