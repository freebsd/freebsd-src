/*-
 * Copyright (c) 2006 Kip Macy
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
 * $FreeBSD: src/sys/sun4v/include/mdesc_bus.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_MACHINE_MDESC_BUS_H_
#define	_MACHINE_MDESC_BUS_H_

#include <sys/bus.h>
#include <machine/mdesc_bus_subr.h>

#include "mdesc_bus_if.h"

static __inline const char *
mdesc_bus_get_compat(device_t dev)
{

	return (MDESC_BUS_GET_COMPAT(device_get_parent(dev), dev));
}

static __inline const struct mdesc_bus_devinfo *
mdesc_bus_get_model(device_t dev)
{

	return (MDESC_BUS_GET_DEVINFO(device_get_parent(dev), dev));
}

static __inline const char *
mdesc_bus_get_name(device_t dev)
{

	return (MDESC_BUS_GET_NAME(device_get_parent(dev), dev));
}

static __inline const char *
mdesc_bus_get_type(device_t dev)
{

	return (MDESC_BUS_GET_TYPE(device_get_parent(dev), dev));
}

static __inline uint64_t
mdesc_bus_get_handle(device_t dev)
{

	return (MDESC_BUS_GET_HANDLE(device_get_parent(dev), dev));
}

#endif /* !_MACHINE_MDESC_BUS_H_ */
