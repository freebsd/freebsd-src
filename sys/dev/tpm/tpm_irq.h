/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kevin Bowling <kbowling@FreeBSD.org>
 */

#ifndef _DEV_TPM_IRQ_H_
#define _DEV_TPM_IRQ_H_

/* TPM_INT_VECTOR selects an LPC SIRQ, not a host interrupt resource. */
#define TPM_IRQ_SIRQ_UNCHANGED	(-1)
#define TPM_IRQ_EVENTS		0x87
#define TPM_IRQ_MODE_MASK	0x18
#define TPM_IRQ_LEVEL_HIGH	0x00
#define TPM_IRQ_LEVEL_LOW	0x08
#define TPM_IRQ_EDGE_RISING	0x10
#define TPM_IRQ_EDGE_FALLING	0x18

/* Bound a lost IRQ's latency and allow a pending handler to be scheduled. */
#define TPM_IRQ_POLL_MS		100
#define TPM_IRQ_GRACE_MS		20

struct tpm_irq_config {
	int		sirq;
	uint32_t	mode;	/* TPM_INT_ENABLE trigger and polarity bits */
	bool		valid;	/* Preserve the allocated route across resume */
};

static inline bool
tpm_irq_supported(uint32_t caps, const struct tpm_irq_config *config)
{

	return ((config->sirq == TPM_IRQ_SIRQ_UNCHANGED ||
	    (config->sirq >= 1 && config->sirq <= 15)) &&
	    (config->mode & ~TPM_IRQ_MODE_MASK) == 0 &&
	    (caps & 1) != 0 && (caps & (8U << (config->mode >> 3))) != 0);
}

int tpm_acpi_configure_irq(device_t, struct resource *,
    struct tpm_irq_config *, bool);

#endif
