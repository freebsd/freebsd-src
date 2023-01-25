/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2023 Elliott Mitchell
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
 */

#ifndef _SYS_INTRTAB_H_
#define _SYS_INTRTAB_H_

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/rman.h>

#include <machine/a_bikeshed_string_for_sed_to_target.h>

/*
 * Initialize the interrupt table
 *
 * newmgr: used to ensure ownership of allocated interrupt numbers.
 */
extern void	intrtab_setup(struct rman *newmgr);

/*
 * Initialize interrupt table internals
 *
 * Call after intrtab_setup() to intialize the internals to an operational
 * state.
 */
extern void	intrtab_init(void);

/*
 * Set the interrupt associated with an interrupt number
 *
 * res: resource indicating ownership of interrupt number.
 * intr: interrupt number to modify.
 * new: pointer to new interrupt.
 * old: pointer to existing interrupt (ensure consistency).
 */
extern int	intrtab_set(struct resource *res, u_int intr, interrupt_t *new,
    const interrupt_t *const old) __result_use_check;

/*
 * Lookup an interrupt number
 *
 * intr: interrupt number to lookup.
 */
extern interrupt_t *intrtab_lookup(u_int intr) __pure;

#endif
