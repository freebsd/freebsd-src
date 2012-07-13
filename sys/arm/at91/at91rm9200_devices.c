/*-
 * Copyright (c) 2012 M. Warner Losh.  All Rights Reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#define	_ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91rm9200var.h>

/*
 * The AT91RM9200 uses the same silicon for both the BGA and PQFP
 * packages.  There's no documented way to detect this at runtime,
 * so we require the board code to register what type of SoC is on the
 * board in question.  The pinouts are not quite compatible, and we
 * use this information to cope with the slight differences.
 */
void
at91rm9200_set_subtype(enum at91_soc_subtype st)
{

	switch (st) {
	case AT91_ST_RM9200_BGA:
	case AT91_ST_RM9200_PQFP:
		soc_info.subtype = st;
		break;
	default:
		panic("Bad SoC subtype %d for at91rm9200_set_subtype.", st);
		break;
	}
}
