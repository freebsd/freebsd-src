/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Bjoern A. Zeeb
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
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <linux/kernel.h>	/* pr_debug */
#include <linux/mhi.h>

static MALLOC_DEFINE(M_LKPIMHI, "lkpimhi", "LinuxKPI MHI compat");

struct mhi_controller *
linuxkpi_mhi_alloc_controller(void)
{
	struct mhi_controller *mhi_ctrl;

	mhi_ctrl = malloc(sizeof(*mhi_ctrl), M_LKPIMHI, M_NOWAIT | M_ZERO);

	return (mhi_ctrl);
}

void
linuxkpi_mhi_free_controller(struct mhi_controller *mhi_ctrl)
{

	/* What else do we need to check that it is gone? */
	free(mhi_ctrl, M_LKPIMHI);
}

int
linuxkpi_mhi_register_controller(struct mhi_controller *mhi_ctrl,
    const struct mhi_controller_config *cfg)
{

	if (mhi_ctrl == NULL || cfg == NULL)
		return (-EINVAL);

#define	CHECK_FIELD(_f)						\
	if (!mhi_ctrl->_f)					\
		return (-ENXIO);
	CHECK_FIELD(cntrl_dev);
	CHECK_FIELD(regs);
	CHECK_FIELD(irq);
	CHECK_FIELD(reg_len);
	CHECK_FIELD(nr_irqs);

	CHECK_FIELD(runtime_get);
	CHECK_FIELD(runtime_put);
	CHECK_FIELD(status_cb);
	CHECK_FIELD(read_reg);
	CHECK_FIELD(write_reg);
#undef CHECK_FIELD

	printf("%s: XXX-BZ TODO\n", __func__);
	return (0);
}

void
linuxkpi_mhi_unregister_controller(struct mhi_controller *mhi_ctrl)
{

	pr_debug("%s: TODO\n", __func__);
}
