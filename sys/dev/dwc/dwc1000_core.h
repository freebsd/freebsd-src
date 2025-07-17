/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#ifndef	__DWC1000_CORE_H__
#define	 __DWC1000_CORE_H__

int dwc1000_miibus_read_reg(device_t dev, int phy, int reg);
int dwc1000_miibus_write_reg(device_t dev, int phy, int reg, int val);
void dwc1000_miibus_statchg(device_t dev);
void dwc1000_core_setup(struct dwc_softc *sc);
int dwc1000_core_reset(struct dwc_softc *sc);
void dwc1000_enable_mac(struct dwc_softc *sc, bool enable);
void dwc1000_enable_csum_offload(struct dwc_softc *sc);
void dwc1000_setup_rxfilter(struct dwc_softc *sc);
void dwc1000_get_hwaddr(struct dwc_softc *sc, uint8_t *hwaddr);
void dwc1000_harvest_stats(struct dwc_softc *sc);
void dwc1000_intr(struct dwc_softc *softc);
void dwc1000_intr_disable(struct dwc_softc *sc);

#endif	/* __DWC1000_CORE_H__ */
