/*-
 * Copyright (c) 2005 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef ARM_AT91_AT91_PMCVAR_H
#define ARM_AT91_AT91_PMCVAR_H

struct at91_pmc_clock 
{
	char		*name;
	uint32_t	hz;
	struct at91_pmc_clock *parent;
	uint32_t	pmc_mask;
	void		(*set_mode)(struct at91_pmc_clock *, int);
	uint32_t	refcnt;
	unsigned	id:2;
	unsigned	primary:1;
	unsigned	pll:1;
	unsigned	programmable:1;

	/* PLL Params */
	uint32_t	pll_min_in;
	uint32_t	pll_max_in;
	uint32_t	pll_min_out;
	uint32_t	pll_max_out;

	uint32_t	pll_div_shift;
	uint32_t	pll_div_mask;
	uint32_t	pll_mul_shift;
	uint32_t	pll_mul_mask;

	uint32_t	(*set_outb)(int);
};

struct at91_pmc_clock * at91_pmc_clock_add(const char *name, uint32_t irq,
    struct at91_pmc_clock *parent);
struct at91_pmc_clock *at91_pmc_clock_ref(const char *name);
void at91_pmc_clock_deref(struct at91_pmc_clock *);
void at91_pmc_clock_enable(struct at91_pmc_clock *);
void at91_pmc_clock_disable(struct at91_pmc_clock *);

#endif /* ARM_AT91_AT91_PMCVAR_H */
