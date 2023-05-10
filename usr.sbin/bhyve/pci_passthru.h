/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#pragma once

#include <vmmapi.h>

#include "pci_emul.h"

struct passthru_mmio_mapping {
	vm_paddr_t gpa; /* guest physical address */
	void *gva;	/* guest virtual address */
	vm_paddr_t hpa; /* host physical address */
	void *hva;	/* guest virtual address */
	vm_paddr_t len;
};

struct passthru_softc;

typedef int (*cfgread_handler)(struct passthru_softc *sc,
    struct pci_devinst *pi, int coff, int bytes, uint32_t *rv);
typedef int (*cfgwrite_handler)(struct passthru_softc *sc,
    struct pci_devinst *pi, int coff, int bytes, uint32_t val);

uint32_t read_config(const struct pcisel *sel, long reg, int width);
void write_config(const struct pcisel *sel, long reg, int width, uint32_t data);
int passthru_cfgread_emulate(struct passthru_softc *sc, struct pci_devinst *pi,
    int coff, int bytes, uint32_t *rv);
int passthru_cfgwrite_emulate(struct passthru_softc *sc, struct pci_devinst *pi,
    int coff, int bytes, uint32_t val);
struct passthru_mmio_mapping *passthru_get_mmio(struct passthru_softc *sc,
    int num);
struct pcisel *passthru_get_sel(struct passthru_softc *sc);
int set_pcir_handler(struct passthru_softc *sc, int reg, int len,
    cfgread_handler rhandler, cfgwrite_handler whandler);
