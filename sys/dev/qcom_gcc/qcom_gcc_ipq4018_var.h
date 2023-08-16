/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
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

#ifndef	__QCOM_GCC_IPQ4018_VAR_H__
#define	__QCOM_GCC_IPQ4018_VAR_H__

struct qcom_gcc_ipq4018_reset_entry {
	uint32_t	reg;
	uint32_t	bit;
};

struct qcom_gcc_ipq4018_softc {
	device_t		dev;
	int			reg_rid;
	struct resource		*reg;
	struct mtx		mtx;
	struct clkdom		*clkdom;
};

/*
 * reset block
 */
extern	int qcom_gcc_ipq4018_hwreset_assert(device_t dev, intptr_t id,
	    bool reset);
extern	int qcom_gcc_ipq4018_hwreset_is_asserted(device_t dev, intptr_t id,
	    bool *reset);

/*
 * clock block
 */
extern	int qcom_gcc_ipq4018_clock_read(device_t dev, bus_addr_t addr,
	    uint32_t *val);
extern	int qcom_gcc_ipq4018_clock_write(device_t dev, bus_addr_t addr,
	    uint32_t val);
extern	int qcom_gcc_ipq4018_clock_modify(device_t dev, bus_addr_t addr,
     uint32_t clear_mask, uint32_t set_mask);
extern	void qcom_gcc_ipq4018_clock_setup(struct qcom_gcc_ipq4018_softc *sc);
extern	void qcom_gcc_ipq4018_clock_lock(device_t dev);
extern	void qcom_gcc_ipq4018_clock_unlock(device_t dev);

#endif	/* __QCOM_GCC_IPQ4018_VAR_H__ */
