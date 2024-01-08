/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#pragma once

#include <sys/linker_set.h>

#include <vmmapi.h>

#include "config.h"
#include "pci_emul.h"

struct passthru_mmio_mapping {
	vm_paddr_t gpa; /* guest physical address */
	void *gva;	/* guest virtual address */
	vm_paddr_t hpa; /* host physical address */
	void *hva;	/* guest virtual address */
	vm_paddr_t len;
};

struct passthru_softc;

struct passthru_dev {
    int (*probe)(struct pci_devinst *pi);
    int (*init)(struct pci_devinst *pi, nvlist_t *nvl);
    void (*deinit)(struct pci_devinst *pi);
};
#define PASSTHRU_DEV_SET(x) DATA_SET(passthru_dev_set, x)

typedef int (*cfgread_handler)(struct passthru_softc *sc,
    struct pci_devinst *pi, int coff, int bytes, uint32_t *rv);
typedef int (*cfgwrite_handler)(struct passthru_softc *sc,
    struct pci_devinst *pi, int coff, int bytes, uint32_t val);
typedef uint64_t (*passthru_read_handler)(struct pci_devinst *pi, int baridx,
    uint64_t offset, int size);
typedef void (*passthru_write_handler)(struct pci_devinst *pi, int baridx, uint64_t offset,
    int size, uint64_t val);

uint32_t pci_host_read_config(const struct pcisel *sel, long reg, int width);
void pci_host_write_config(const struct pcisel *sel, long reg, int width,
    uint32_t data);

int passthru_cfgread_emulate(struct passthru_softc *sc, struct pci_devinst *pi,
    int coff, int bytes, uint32_t *rv);
int passthru_cfgwrite_emulate(struct passthru_softc *sc, struct pci_devinst *pi,
    int coff, int bytes, uint32_t val);
struct passthru_mmio_mapping *passthru_get_mmio(struct passthru_softc *sc,
    int num);
struct pcisel *passthru_get_sel(struct passthru_softc *sc);
int set_pcir_handler(struct passthru_softc *sc, int reg, int len,
    cfgread_handler rhandler, cfgwrite_handler whandler);
int passthru_set_bar_handler(struct passthru_softc *sc, int baridx,
    uint64_t off, uint64_t size, passthru_read_handler rhandler,
    passthru_write_handler whandler);
