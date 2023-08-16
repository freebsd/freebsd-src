/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef QAT_FREEBSD_H_
#define QAT_FREEBSD_H_

#include <sys/param.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/firmware.h>
#include <sys/rman.h>
#include <sys/types.h>
#include <sys/ctype.h>
#include <sys/ioccom.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <machine/bus.h>
#include <machine/bus_dma.h>
#include <sys/firmware.h>
#include <asm/uaccess.h>
#include <linux/math64.h>
#include <linux/spinlock.h>

#define PCI_VENDOR_ID_INTEL 0x8086

#if !defined(__bool_true_false_are_defined)
#define __bool_true_false_are_defined 1
#define false 0
#define true 1
#if __STDC_VERSION__ < 199901L && __GNUC__ < 3 && !defined(__INTEL_COMPILER)
typedef int _Bool;
#endif
typedef _Bool bool;
#endif /* !__bool_true_false_are_defined && !__cplusplus */

#if __STDC_VERSION__ < 199901L && __GNUC__ < 3 && !defined(__INTEL_COMPILER)
typedef int _Bool;
#endif

#define pause_ms(wmesg, ms) pause_sbt(wmesg, (ms)*SBT_1MS, 0, C_HARDCLOCK)

/* Function sets the MaxPayload size of a PCI device. */
int pci_set_max_payload(device_t dev, int payload_size);

device_t pci_find_pf(device_t vf);

MALLOC_DECLARE(M_QAT);

struct msix_entry {
	struct resource *irq;
	void *cookie;
};

struct pci_device_id {
	uint16_t vendor;
	uint16_t device;
};

struct bus_dmamem {
	bus_dma_tag_t dma_tag;
	bus_dmamap_t dma_map;
	void *dma_vaddr;
	bus_addr_t dma_baddr;
};

/*
 * Allocate a mapping.	On success, zero is returned and the 'dma_vaddr'
 * and 'dma_baddr' fields are populated with the virtual and bus addresses,
 * respectively, of the mapping.
 */
int bus_dma_mem_create(struct bus_dmamem *mem,
		       bus_dma_tag_t parent,
		       bus_size_t alignment,
		       bus_addr_t lowaddr,
		       bus_size_t len,
		       int flags);

/*
 * Release a mapping created by bus_dma_mem_create().
 */
void bus_dma_mem_free(struct bus_dmamem *mem);

#define list_for_each_prev_safe(p, n, h)                                       \
	for (p = (h)->prev, n = (p)->prev; p != (h); p = n, n = (p)->prev)

static inline int
compat_strtoul(const char *cp, unsigned int base, unsigned long *res)
{
	char *end;

	*res = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	return (0);
}

static inline int
compat_strtouint(const char *cp, unsigned int base, unsigned int *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return (-EINVAL);
	if (temp != (unsigned int)temp)
		return (-ERANGE);
	return (0);
}

static inline int
compat_strtou8(const char *cp, unsigned int base, unsigned char *res)
{
	char *end;
	unsigned long temp;

	*res = temp = strtoul(cp, &end, base);

	/* skip newline character, if any */
	if (*end == '\n')
		end++;
	if (*cp == 0 || *end != 0)
		return -EINVAL;
	if (temp != (unsigned char)temp)
		return -ERANGE;
	return 0;
}

#if __FreeBSD_version >= 1300500
#undef dev_to_node
static inline int
dev_to_node(device_t dev)
{
	int numa_domain;

	if (!dev || bus_get_domain(dev, &numa_domain) != 0)
		return (-1);
	else
		return (numa_domain);
}
#endif
#endif
