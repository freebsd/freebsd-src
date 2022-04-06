/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#pragma once

#include "qemu_fwcfg.h"

struct qemu_loader;

/*
 * Some guest bios like seabios assume the RSDP to be located in the FSEG. Bhyve
 * only supports OVMF which has no such requirement.
 */
enum qemu_loader_zone {
	QEMU_LOADER_ALLOC_HIGH = 1,
	QEMU_LOADER_ALLOC_FSEG, /* 0x0F000000 - 0x100000 */
};

/**
 * Loads a fwcfg item into guest memory. This command has to be issued before
 * any subsequent command can be used.
 *
 * @param loader     Qemu loader instance the command should be added to.
 * @param name       Name of the fwcfg item which should be allocated.
 * @param alignment  Alignment required by the data.
 * @param zone       Memory zone in which it should be loaded.
 */
int qemu_loader_alloc(struct qemu_loader *loader, const uint8_t *name,
    uint32_t alignment, enum qemu_loader_zone zone);
/**
 * Calculates a checksum for @p name and writes it to @p name + @p off . The
 * checksum calculation ranges from @p start to @p start + @p len. The checksum
 * field is always one byte large and all bytes in the specified range,
 * including the checksum, have to sum up to 0.
 *
 * @param loader Qemu loader instance the command should be added to.
 * @param name   Name of the fwcfg item which should be patched.
 * @param off    Offset into @p name .
 * @param start  Start offset of checksum calculation.
 * @param len    Length of the checksum calculation.
 */
int qemu_loader_add_checksum(struct qemu_loader *loader, const uint8_t *name,
    uint32_t off, uint32_t start, uint32_t len);
/**
 * Adds the address of @p src_name to the value at @p dest_name + @p off . The
 * size of the pointer is determined by @p dest_size and should be 1, 2, 4 or 8.
 *
 * @param loader     Qemu loader instance the command should be added to.
 * @param dest_name  Name of the fwcfg item which should be patched.
 * @param src_name   Name of the fwcfg item which address should be written to
 *                   @p dest_name + @p off.
 * @param off        Offset into @p dest_name .
 * @param size       Size of the pointer (1, 2, 4 or 8).
 */
int qemu_loader_add_pointer(struct qemu_loader *loader,
    const uint8_t *dest_name, const uint8_t *src_name, uint32_t off,
    uint8_t size);

/**
 * Creates a qemu loader instance.
 *
 * @param new_loader  Returns the newly created qemu loader instance.
 * @param fwcfg_name  Name of the FwCfg item which represents the qemu loader
 */
int qemu_loader_create(struct qemu_loader **new_loader,
    const uint8_t *fwcfg_name);
/**
 * Signals that all commands are written to the qemu loader. This function
 * creates a proper FwCfg item and registers it.
 *
 * @param loader  Qemu loader instance which should be finished.
 */
int qemu_loader_finish(struct qemu_loader *loader);
