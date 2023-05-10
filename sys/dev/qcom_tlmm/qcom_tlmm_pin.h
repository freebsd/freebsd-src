/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 */

#ifndef	__QCOM_TLMM_PIN_H__
#define	__QCOM_TLMM_PIN_H__

extern	device_t qcom_tlmm_get_bus(device_t dev);
extern	int qcom_tlmm_pin_max(device_t dev, int *maxpin);
extern	int qcom_tlmm_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps);
extern	int qcom_tlmm_pin_getflags(device_t dev, uint32_t pin,
	    uint32_t *flags);
extern	int qcom_tlmm_pin_getname(device_t dev, uint32_t pin, char *name);
extern	int qcom_tlmm_pin_setflags(device_t dev, uint32_t pin,
	    uint32_t flags);
extern	int qcom_tlmm_pin_set(device_t dev, uint32_t pin, unsigned int value);
extern	int qcom_tlmm_pin_get(device_t dev, uint32_t pin, unsigned int *val);
extern	int qcom_tlmm_pin_toggle(device_t dev, uint32_t pin);
extern	int qcom_tlmm_filter(void *arg);
extern	void qcom_tlmm_intr(void *arg);
extern	phandle_t qcom_tlmm_pin_get_node(device_t dev, device_t bus);

#endif	/* __QCOM_TLMM_PIN_H__ */
