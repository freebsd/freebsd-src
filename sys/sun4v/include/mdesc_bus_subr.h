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
 * $FreeBSD: src/sys/sun4v/include/mdesc_bus_subr.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_MDESC_MDESC_BUS_SUBR_H_
#define _MACHINE_MDESC_MDESC_BUS_SUBR_H_

#include <sys/bus.h>

#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

#include "mdesc_bus_if.h"

int     mdesc_bus_gen_setup_devinfo(struct mdesc_bus_devinfo *, mde_cookie_t);
void    mdesc_bus_gen_destroy_devinfo(struct mdesc_bus_devinfo *);

mdesc_bus_get_compat_t    mdesc_bus_gen_get_compat;
mdesc_bus_get_name_t      mdesc_bus_gen_get_name;
mdesc_bus_get_type_t      mdesc_bus_gen_get_type;
mdesc_bus_get_handle_t    mdesc_bus_gen_get_handle;

#endif /* !_MACHINE_MDESC_MDESC_BUS_SUBR_H_ */
