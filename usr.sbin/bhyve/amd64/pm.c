/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Hudson River Trading LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
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
 */

#include <sys/types.h>
#include <machine/vmm.h>

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <vmmapi.h>

#include "acpi.h"
#include "inout.h"
#include "mevent.h"
#include "pci_irq.h"
#include "pci_lpc.h"

static pthread_mutex_t pm_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mevent *power_button;
static sig_t old_power_handler;

static unsigned gpe0_active;
static unsigned gpe0_enabled;
static const unsigned gpe0_valid = (1u << GPE_VMGENC);

/*
 * Reset Control register at I/O port 0xcf9.  Bit 2 forces a system
 * reset when it transitions from 0 to 1.  Bit 1 selects the type of
 * reset to attempt: 0 selects a "soft" reset, and 1 selects a "hard"
 * reset.
 */
static int
reset_handler(struct vmctx *ctx __unused, int in,
    int port __unused, int bytes, uint32_t *eax, void *arg __unused)
{
	int error;

	static uint8_t reset_control;

	if (bytes != 1)
		return (-1);
	if (in)
		*eax = reset_control;
	else {
		reset_control = *eax;

		/* Treat hard and soft resets the same. */
		if (reset_control & 0x4) {
			error = vm_suspend(ctx, VM_SUSPEND_RESET);
			assert(error == 0 || errno == EALREADY);
		}
	}
	return (0);
}
INOUT_PORT(reset_reg, 0xCF9, IOPORT_F_INOUT, reset_handler);

/*
 * ACPI's SCI is a level-triggered interrupt.
 */
static int sci_active;

static void
sci_assert(struct vmctx *ctx)
{

	if (sci_active)
		return;
	vm_isa_assert_irq(ctx, SCI_INT, SCI_INT);
	sci_active = 1;
}

static void
sci_deassert(struct vmctx *ctx)
{

	if (!sci_active)
		return;
	vm_isa_deassert_irq(ctx, SCI_INT, SCI_INT);
	sci_active = 0;
}

/*
 * Power Management 1 Event Registers
 *
 * The only power management event supported is a power button upon
 * receiving SIGTERM.
 */
static uint16_t pm1_enable, pm1_status;

#define	PM1_TMR_STS		0x0001
#define	PM1_BM_STS		0x0010
#define	PM1_GBL_STS		0x0020
#define	PM1_PWRBTN_STS		0x0100
#define	PM1_SLPBTN_STS		0x0200
#define	PM1_RTC_STS		0x0400
#define	PM1_WAK_STS		0x8000

#define	PM1_TMR_EN		0x0001
#define	PM1_GBL_EN		0x0020
#define	PM1_PWRBTN_EN		0x0100
#define	PM1_SLPBTN_EN		0x0200
#define	PM1_RTC_EN		0x0400

static void
sci_update(struct vmctx *ctx)
{
	int need_sci;

	/* See if the SCI should be active or not. */
	need_sci = 0;
	if ((pm1_enable & PM1_TMR_EN) && (pm1_status & PM1_TMR_STS))
		need_sci = 1;
	if ((pm1_enable & PM1_GBL_EN) && (pm1_status & PM1_GBL_STS))
		need_sci = 1;
	if ((pm1_enable & PM1_PWRBTN_EN) && (pm1_status & PM1_PWRBTN_STS))
		need_sci = 1;
	if ((pm1_enable & PM1_SLPBTN_EN) && (pm1_status & PM1_SLPBTN_STS))
		need_sci = 1;
	if ((pm1_enable & PM1_RTC_EN) && (pm1_status & PM1_RTC_STS))
		need_sci = 1;
	if ((gpe0_enabled & gpe0_active) != 0)
		need_sci = 1;

	if (need_sci)
		sci_assert(ctx);
	else
		sci_deassert(ctx);
}

static int
pm1_status_handler(struct vmctx *ctx, int in,
    int port __unused, int bytes, uint32_t *eax, void *arg __unused)
{

	if (bytes != 2)
		return (-1);

	pthread_mutex_lock(&pm_lock);
	if (in)
		*eax = pm1_status;
	else {
		/*
		 * Writes are only permitted to clear certain bits by
		 * writing 1 to those flags.
		 */
		pm1_status &= ~(*eax & (PM1_WAK_STS | PM1_RTC_STS |
		    PM1_SLPBTN_STS | PM1_PWRBTN_STS | PM1_BM_STS));
		sci_update(ctx);
	}
	pthread_mutex_unlock(&pm_lock);
	return (0);
}

static int
pm1_enable_handler(struct vmctx *ctx, int in,
    int port __unused, int bytes, uint32_t *eax, void *arg __unused)
{

	if (bytes != 2)
		return (-1);

	pthread_mutex_lock(&pm_lock);
	if (in)
		*eax = pm1_enable;
	else {
		/*
		 * Only permit certain bits to be set.  We never use
		 * the global lock, but ACPI-CA whines profusely if it
		 * can't set GBL_EN.
		 */
		pm1_enable = *eax & (PM1_RTC_EN | PM1_PWRBTN_EN | PM1_GBL_EN);
		sci_update(ctx);
	}
	pthread_mutex_unlock(&pm_lock);
	return (0);
}
INOUT_PORT(pm1_status, PM1A_EVT_ADDR, IOPORT_F_INOUT, pm1_status_handler);
INOUT_PORT(pm1_enable, PM1A_EVT_ADDR + 2, IOPORT_F_INOUT, pm1_enable_handler);

static void
power_button_handler(int signal __unused, enum ev_type type __unused, void *arg)
{
	struct vmctx *ctx;

	ctx = arg;
	pthread_mutex_lock(&pm_lock);
	if (!(pm1_status & PM1_PWRBTN_STS)) {
		pm1_status |= PM1_PWRBTN_STS;
		sci_update(ctx);
	}
	pthread_mutex_unlock(&pm_lock);
}

/*
 * Power Management 1 Control Register
 *
 * This is mostly unimplemented except that we wish to handle writes that
 * set SPL_EN to handle S5 (soft power off).
 */
static uint16_t pm1_control;

#define	PM1_SCI_EN	0x0001
#define	PM1_SLP_TYP	0x1c00
#define	PM1_SLP_EN	0x2000
#define	PM1_ALWAYS_ZERO	0xc003

static int
pm1_control_handler(struct vmctx *ctx, int in,
    int port __unused, int bytes, uint32_t *eax, void *arg __unused)
{
	int error;

	if (bytes != 2)
		return (-1);
	if (in)
		*eax = pm1_control;
	else {
		/*
		 * Various bits are write-only or reserved, so force them
		 * to zero in pm1_control.  Always preserve SCI_EN as OSPM
		 * can never change it.
		 */
		pm1_control = (pm1_control & PM1_SCI_EN) |
		    (*eax & ~(PM1_SLP_EN | PM1_ALWAYS_ZERO));

		/*
		 * If SLP_EN is set, check for S5.  Bhyve's _S5_ method
		 * says that '5' should be stored in SLP_TYP for S5.
		 */
		if (*eax & PM1_SLP_EN) {
			if ((pm1_control & PM1_SLP_TYP) >> 10 == 5) {
				error = vm_suspend(ctx, VM_SUSPEND_POWEROFF);
				assert(error == 0 || errno == EALREADY);
			}
		}
	}
	return (0);
}
INOUT_PORT(pm1_control, PM1A_CNT_ADDR, IOPORT_F_INOUT, pm1_control_handler);
SYSRES_IO(PM1A_EVT_ADDR, 8);

void
acpi_raise_gpe(struct vmctx *ctx, unsigned bit)
{
	unsigned mask;

	assert(bit < (IO_GPE0_LEN * (8 / 2)));
	mask = (1u << bit);
	assert((mask & ~gpe0_valid) == 0);

	pthread_mutex_lock(&pm_lock);
	gpe0_active |= mask;
	sci_update(ctx);
	pthread_mutex_unlock(&pm_lock);
}

static int
gpe0_sts(struct vmctx *ctx, int in, int port __unused,
    int bytes, uint32_t *eax, void *arg __unused)
{
	/*
	 * ACPI 6.2 specifies the GPE register blocks are accessed
	 * byte-at-a-time.
	 */
	if (bytes != 1)
		return (-1);

	pthread_mutex_lock(&pm_lock);
	if (in)
		*eax = gpe0_active;
	else {
		/* W1C */
		gpe0_active &= ~(*eax & gpe0_valid);
		sci_update(ctx);
	}
	pthread_mutex_unlock(&pm_lock);
	return (0);
}
INOUT_PORT(gpe0_sts, IO_GPE0_STS, IOPORT_F_INOUT, gpe0_sts);

static int
gpe0_en(struct vmctx *ctx, int in, int port __unused,
    int bytes, uint32_t *eax, void *arg __unused)
{
	if (bytes != 1)
		return (-1);

	pthread_mutex_lock(&pm_lock);
	if (in)
		*eax = gpe0_enabled;
	else {
		gpe0_enabled = (*eax & gpe0_valid);
		sci_update(ctx);
	}
	pthread_mutex_unlock(&pm_lock);
	return (0);
}
INOUT_PORT(gpe0_en, IO_GPE0_EN, IOPORT_F_INOUT, gpe0_en);

/*
 * ACPI SMI Command Register
 *
 * This write-only register is used to enable and disable ACPI.
 */
static int
smi_cmd_handler(struct vmctx *ctx, int in, int port __unused,
    int bytes, uint32_t *eax, void *arg __unused)
{

	assert(!in);
	if (bytes != 1)
		return (-1);

	pthread_mutex_lock(&pm_lock);
	switch (*eax) {
	case BHYVE_ACPI_ENABLE:
		pm1_control |= PM1_SCI_EN;
		if (power_button == NULL) {
			power_button = mevent_add(SIGTERM, EVF_SIGNAL,
			    power_button_handler, ctx);
			old_power_handler = signal(SIGTERM, SIG_IGN);
		}
		break;
	case BHYVE_ACPI_DISABLE:
		pm1_control &= ~PM1_SCI_EN;
		if (power_button != NULL) {
			mevent_delete(power_button);
			power_button = NULL;
			signal(SIGTERM, old_power_handler);
		}
		break;
	}
	pthread_mutex_unlock(&pm_lock);
	return (0);
}
INOUT_PORT(smi_cmd, SMI_CMD, IOPORT_F_OUT, smi_cmd_handler);
SYSRES_IO(SMI_CMD, 1);

void
sci_init(struct vmctx *ctx)
{

	/*
	 * Mark ACPI's SCI as level trigger and bump its use count
	 * in the PIRQ router.
	 */
	pci_irq_use(SCI_INT);
	vm_isa_set_irq_trigger(ctx, SCI_INT, LEVEL_TRIGGER);
}
