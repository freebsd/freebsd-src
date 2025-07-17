/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef	_SDHCI_FDT_H_
#define	_SDHCI_FDT_H_

#define	SDHCI_FDT_MAX_SLOTS		6

struct sdhci_fdt_softc {
	device_t	dev;		/* Controller device */
	u_int		quirks;		/* Chip specific quirks */
	u_int		caps;		/* If we override SDHCI_CAPABILITIES */
	uint32_t	max_clk;	/* Max possible freq */
	uint8_t		sdma_boundary;	/* If we override the SDMA boundary */
	struct resource *irq_res;	/* IRQ resource */
	void		*intrhand;	/* Interrupt handle */

	int		num_slots;	/* Number of slots on this controller*/
	struct sdhci_slot slots[SDHCI_FDT_MAX_SLOTS];
	struct resource	*mem_res[SDHCI_FDT_MAX_SLOTS];	/* Memory resource */

	bool		wp_inverted;	/* WP pin is inverted */
	bool		wp_disabled;	/* WP pin is not supported */
	bool		no_18v;		/* No 1.8V support */

	clk_t		clk_xin;	/* xin24m fixed clock */
	clk_t		clk_ahb;	/* ahb clock */
	clk_t		clk_core;	/* core clock */
	phy_t		phy;		/* phy to be used */

	struct syscon	*syscon;	/* Handle to the syscon */
};

int sdhci_fdt_attach(device_t dev);
int sdhci_fdt_detach(device_t dev);
int sdhci_get_syscon(struct sdhci_fdt_softc *sc);
int sdhci_init_phy(struct sdhci_fdt_softc *sc);
void sdhci_export_clocks(struct sdhci_fdt_softc *sc);
int sdhci_clock_ofw_map(struct clkdom *clkdom, uint32_t ncells,
    phandle_t *cells, struct clknode **clk);
int sdhci_init_clocks(device_t dev);
int sdhci_fdt_set_clock(device_t dev, struct sdhci_slot *slot,
    int clock);
#endif
