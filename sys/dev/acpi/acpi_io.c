/*-
 * Copyright (c) 1999 Takanori Watanabe <takawata@shidahara1.planet.sci.kobe-u.ac.jp>
 * Copyright (c) 1999, 2000 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 *	$FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

/*
 * ACPI Register I/O
 */
static __inline void
acpi_register_input(acpi_softc_t *sc, int res, int offset, u_int32_t *value, u_int32_t size)
{
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	u_int32_t		val;

	if (sc->iores[res].rsc == NULL)
		return;

	bst = sc->iores[res].btag;
	bsh = sc->iores[res].bhandle;

	switch (size) {
	case 1:
		val = bus_space_read_1(bst, bsh, offset);
		break;
	case 2:
		val = bus_space_read_2(bst, bsh, offset);
		break;
	case 3:
		val = bus_space_read_4(bst, bsh, offset);
		val &= 0x00ffffff;
		break;
	case 4:
		val = bus_space_read_4(bst, bsh, offset);
		break;
	default:
		ACPI_DEVPRINTF("acpi_register_input(): invalid size (%d)\n", size);
		val = 0;
		break;
	}

	*value = val;
}

static __inline void
acpi_register_output(acpi_softc_t *sc, int res, int offset, u_int32_t *value, u_int32_t size)
{
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	u_int32_t		val;

	if (sc->iores[res].rsc == NULL)
		return;

	val = *value;
	bst = sc->iores[res].btag;
	bsh = sc->iores[res].bhandle;

	switch (size) {
	case 1:
		bus_space_write_1(bst, bsh, offset, val & 0xff);
		break;
	case 2:
		bus_space_write_2(bst, bsh, offset, val & 0xffff);
		break;
	case 3:
		bus_space_write_2(bst, bsh, offset, val & 0xffff);
		bus_space_write_1(bst, bsh, offset + 2, (val >> 16) & 0xff);
		break;
	case 4:
		bus_space_write_4(bst, bsh, offset, val);
		break;
	default:
		ACPI_DEVPRINTF("acpi_register_output(): invalid size\n");
		break;
	}
}

static __inline void
acpi_io_mirreg(acpi_softc_t *sc, boolean_t io, u_int32_t *data, 
	       int res, int altres, int offset, int size)
{
	u_int32_t	result;

	if (io == ACPI_REGISTER_INPUT) {
		acpi_register_input(sc, res, offset, &result, size);
		*data = result;
		acpi_register_input(sc, altres, offset, &result, size);
		*data |= result;
	} else {
		acpi_register_output(sc, res, offset, data, size);
		acpi_register_output(sc, altres, offset, data, size);
	}

	return;
}

void
acpi_enable_disable(acpi_softc_t *sc, boolean_t enable)
{
	u_int8_t	val;

	val = enable ? sc->facp_body->acpi_enable : sc->facp_body->acpi_disable;
	bus_space_write_1(sc->iores[ACPI_RES_SMI_CMD].btag,
			  sc->iores[ACPI_RES_SMI_CMD].bhandle,
			  0, val);
	sc->enabled = enable;

	ACPI_DEBUGPRINT("acpi_enable_disable(%d) = (%x)\n", enable, val);
}

void
acpi_io_pm1_status(acpi_softc_t *sc, boolean_t io, u_int32_t *status)
{
	int		size;
	struct FACPbody	*facp;

	facp = sc->facp_body;
	size = facp->pm1_evt_len / 2;
	acpi_io_mirreg(sc, io, status, ACPI_RES_PM1A_EVT, ACPI_RES_PM1B_EVT, 0, size);

	ACPI_DEBUGPRINT("acpi_io_pm1_status(%d) = (%x)\n", io, *status);
}

void
acpi_io_pm1_enable(acpi_softc_t *sc, boolean_t io, u_int32_t *enable)
{
	int		size;
	struct FACPbody	*facp;

	facp = sc->facp_body;
	size = facp->pm1_evt_len / 2;
	acpi_io_mirreg(sc, io, enable, ACPI_RES_PM1A_EVT, ACPI_RES_PM1B_EVT, size, size);

	ACPI_DEBUGPRINT("acpi_io_pm1_enable(%d) = (%x)\n", io, *enable);
}

/*
 * PM1 is awkward because the SLP_TYP bits are not common between the two registers.
 * A better interface than this might pass the SLP_TYP bits separately.
 */
void
acpi_io_pm1_control(acpi_softc_t *sc, boolean_t io, u_int32_t *value_a, u_int32_t *value_b)
{
	struct FACPbody	*facp;
	u_int32_t	result;

	facp = sc->facp_body;

	if (io == ACPI_REGISTER_INPUT) {
		acpi_register_input(sc, ACPI_RES_PM1A_CNT, 0, &result, facp->pm1_cnt_len);
		*value_a = result;
		acpi_register_input(sc, ACPI_RES_PM1B_CNT, 0, &result, facp->pm1_cnt_len);
		*value_a |= result;
		*value_a &= ~ACPI_CNT_SLP_TYPX;	/* mask the SLP_TYP bits */
	} else {
		acpi_register_output(sc, ACPI_RES_PM1A_CNT, 0, value_a, facp->pm1_cnt_len);
		acpi_register_output(sc, ACPI_RES_PM1B_CNT, 0, value_b, facp->pm1_cnt_len);
	}

	ACPI_DEBUGPRINT("acpi_io_pm1_control(%d) = (%x, %x)\n", io, *value_a, *value_b);
}

void
acpi_io_pm2_control(acpi_softc_t *sc, boolean_t io, u_int32_t *val)
{
	int		size;
	struct FACPbody	*facp;

	facp = sc->facp_body;
	size = facp->pm2_cnt_len;

	if (size == 0)			/* port is optional */
	    return;

	if (io == ACPI_REGISTER_INPUT) {
		acpi_register_input(sc, ACPI_RES_PM2_CNT, 0, val, size);
	} else {
		acpi_register_output(sc, ACPI_RES_PM2_CNT, 0, val, size);
	}

	ACPI_DEBUGPRINT("acpi_io_pm2_control(%d) = (%x)\n", io, *val);

	return;
}

void
acpi_io_pm_timer(acpi_softc_t *sc, boolean_t io, u_int32_t *val)
{
	if (io == ACPI_REGISTER_INPUT) {
		acpi_register_input(sc, ACPI_RES_PM_TMR, 0, val, sizeof(u_int32_t));

		ACPI_DEBUGPRINT("acpi_io_pm_timer(%d) = (%x)\n", io, *val);
	}
}

void
acpi_io_gpe0_status(acpi_softc_t *sc, boolean_t io, u_int32_t *val)
{
	int		size;
	struct	FACPbody *facp;

	facp = sc->facp_body;
	size = facp->gpe0_len / 2;

	if (size == 0)			/* port is optional */
	    return;

	if (io == ACPI_REGISTER_INPUT) {
		acpi_register_input(sc, ACPI_RES_GPE0, 0, val, size);
	} else {
		acpi_register_output(sc, ACPI_RES_GPE0, 0, val, size);
	}

	ACPI_DEBUGPRINT("acpi_io_gpe0_status(%d) = (%x)\n", io, *val);
}

void
acpi_io_gpe0_enable(acpi_softc_t *sc, boolean_t io, u_int32_t *val)
{
	int		size;
	struct	FACPbody *facp;

	facp = sc->facp_body;
	size = facp->gpe0_len / 2;

	if (size == 0)			/* port is optional */
	    return;

	if (io == ACPI_REGISTER_INPUT) {
		acpi_register_input(sc, ACPI_RES_GPE0, size, val, size);
	} else {
		acpi_register_output(sc, ACPI_RES_GPE0, size, val, size);
	}

	ACPI_DEBUGPRINT("acpi_io_gpe0_enable(%d) = (%x)\n", io, *val);
}

void
acpi_io_gpe1_status(acpi_softc_t *sc, boolean_t io, u_int32_t *val)
{
	int		size;
	struct	FACPbody *facp;

	facp = sc->facp_body;
	size = facp->gpe1_len / 2;

	if (size == 0)			/* port is optional */
	    return;

	if (io == ACPI_REGISTER_INPUT) {
		acpi_register_input(sc, ACPI_RES_GPE1, 0, val, size);
	} else {
		acpi_register_output(sc, ACPI_RES_GPE1, 0, val, size);
	}

	ACPI_DEBUGPRINT("acpi_io_gpe1_status(%d) = (%x)\n", io, *val);
}

void
acpi_io_gpe1_enable(acpi_softc_t *sc, boolean_t io, u_int32_t *val)
{
	int		size;
	struct	FACPbody *facp;

	facp = sc->facp_body;
	size = facp->gpe1_len / 2;

	if (size == 0)			/* port is optional */
	    return;

	if (io == ACPI_REGISTER_INPUT) {
		acpi_register_input(sc, ACPI_RES_GPE1, size, val, size);
	} else {
		acpi_register_output(sc, ACPI_RES_GPE1, size, val, size);
	}

	ACPI_DEBUGPRINT("acpi_io_gpe0_enable(%d) = (%x)\n", io, *val);
}

void
acpi_gpe_enable_bit(acpi_softc_t *sc, u_int32_t bit, boolean_t on_off)
{
	u_int32_t	value;
	int		res;

	/*
	 * Is the bit in the first GPE port?
	 */
	if (bit < ((sc->facp_body->gpe0_len / 2) * 8)) {
		res = ACPI_RES_GPE0;
	} else {
		/*
		 * Is the bit in the second GPE port?
		 */
		bit -= sc->facp_body->gpe1_base;
		if (bit < ((sc->facp_body->gpe1_len / 2) * 8)) {
			res = ACPI_RES_GPE1;
		} else {
			return;	/* do nothing */
		}
	}

	switch (res) {
	case ACPI_RES_GPE0:
		acpi_io_gpe0_enable(sc, ACPI_REGISTER_INPUT, &value);
		break;
	case ACPI_RES_GPE1:
		acpi_io_gpe1_enable(sc, ACPI_REGISTER_INPUT, &value);
		break;
	}
	value = (value & ~(1 << bit)) | (on_off ? (1 << bit) : 0);
	switch (res) {
	case ACPI_RES_GPE0:
		acpi_io_gpe0_enable(sc, ACPI_REGISTER_OUTPUT, &value);
		break;
	case ACPI_RES_GPE1:
		acpi_io_gpe1_enable(sc, ACPI_REGISTER_OUTPUT, &value);
		break;
	}
}

