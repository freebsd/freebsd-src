/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
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

#ifndef __GPIOBUS_INTERNAL_H__
#define __GPIOBUS_INTERNAL_H__

/*
 * Functions shared between gpiobus and other bus classes that derive from it;
 * these should not be called directly by other drivers.
 */
int gpiobus_attach(device_t);
int gpiobus_detach(device_t);
int gpiobus_init_softc(device_t);
int gpiobus_alloc_ivars(struct gpiobus_ivar *);
void gpiobus_free_ivars(struct gpiobus_ivar *);
int gpiobus_read_ivar(device_t, device_t, int, uintptr_t *);
int gpiobus_acquire_pin(device_t, uint32_t);
void gpiobus_release_pin(device_t, uint32_t);
int gpiobus_child_location(device_t, device_t, struct sbuf *);
device_t gpiobus_add_child_common(device_t, u_int, const char *, int, size_t);
int gpiobus_add_gpioc(device_t);

extern driver_t gpiobus_driver;
#endif
