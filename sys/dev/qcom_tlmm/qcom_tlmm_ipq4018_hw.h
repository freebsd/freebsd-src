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

#ifndef	__QCOM_TLMM_IPQ4018_HW_H__
#define	__QCOM_TLMM_IPQ4018_HW_H__

extern	int qcom_tlmm_ipq4018_hw_pin_set_function(
	    struct qcom_tlmm_softc *sc, int pin, int function);
extern	int qcom_tlmm_ipq4018_hw_pin_get_function(
	    struct qcom_tlmm_softc *sc, int pin, int *function);

extern	int qcom_tlmm_ipq4018_hw_pin_set_oe_output(
	    struct qcom_tlmm_softc *sc, int pin);
extern	int qcom_tlmm_ipq4018_hw_pin_set_oe_input(
	    struct qcom_tlmm_softc *sc, int pin);
extern	int qcom_tlmm_ipq4018_hw_pin_get_oe_state(
	    struct qcom_tlmm_softc *sc, int pin, bool *is_output);

extern	int qcom_tlmm_ipq4018_hw_pin_set_output_value(
	    struct qcom_tlmm_softc *sc,
	    uint32_t pin, unsigned int value);
extern	int qcom_tlmm_ipq4018_hw_pin_get_output_value(
	    struct qcom_tlmm_softc *sc,
	    uint32_t pin, unsigned int *val);
extern	int qcom_tlmm_ipq4018_hw_pin_get_input_value(
	    struct qcom_tlmm_softc *sc,
	    uint32_t pin, unsigned int *val);
extern	int qcom_tlmm_ipq4018_hw_pin_toggle_output_value(
	    struct qcom_tlmm_softc *sc,
	    uint32_t pin);

extern	int qcom_tlmm_ipq4018_hw_pin_set_pupd_config(
	    struct qcom_tlmm_softc *sc, uint32_t pin,
	    qcom_tlmm_pin_pupd_config_t pupd);
extern	int qcom_tlmm_ipq4018_hw_pin_get_pupd_config(
	    struct qcom_tlmm_softc *sc, uint32_t pin,
	    qcom_tlmm_pin_pupd_config_t *pupd);

extern	int qcom_tlmm_ipq4018_hw_pin_set_drive_strength(
	    struct qcom_tlmm_softc *sc, uint32_t pin,
	    uint8_t drv);
extern	int qcom_tlmm_ipq4018_hw_pin_get_drive_strength(
	    struct qcom_tlmm_softc *sc, uint32_t pin,
	    uint8_t *drv);

extern	int qcom_tlmm_ipq4018_hw_pin_set_vm(
	    struct qcom_tlmm_softc *sc, uint32_t pin,
	    bool enable);
extern	int qcom_tlmm_ipq4018_hw_pin_get_vm(
	    struct qcom_tlmm_softc *sc, uint32_t pin,
	    bool *enable);

extern	int qcom_tlmm_ipq4018_hw_pin_set_open_drain(
	    struct qcom_tlmm_softc *sc, uint32_t pin,
	    bool enable);
extern	int qcom_tlmm_ipq4018_hw_pin_get_open_drain(
	    struct qcom_tlmm_softc *sc, uint32_t pin,
	    bool *enable);

#endif	/* __QCOM_TLMM_IPQ4018_HW_H__ */
