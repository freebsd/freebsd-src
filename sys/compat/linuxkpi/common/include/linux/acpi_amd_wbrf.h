/*-
 * Copyright (c) 2025 The FreeBSD Foundation
 * Copyright (c) 2025 Jean-Sébastien Pédron
 *
 * This software was developed by Jean-Sébastien Pédron under sponsorship
 * from the FreeBSD Foundation.
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

#ifndef	_LINUXKPI_LINUX_ACPI_AMD_WBRF_H_
#define	_LINUXKPI_LINUX_ACPI_AMD_WBRF_H_

#include <linux/device.h>
#include <linux/notifier.h>

#define	MAX_NUM_OF_WBRF_RANGES		11

#define	WBRF_RECORD_ADD		0x0
#define	WBRF_RECORD_REMOVE	0x1

struct freq_band_range {
	uint64_t	start;
	uint64_t	end;
};

struct wbrf_ranges_in_out {
	uint64_t		num_of_ranges;
	struct freq_band_range	band_list[MAX_NUM_OF_WBRF_RANGES];
};

enum wbrf_notifier_actions {
	WBRF_CHANGED,
};

/*
 * The following functions currently have dummy implementations that, on Linux,
 * are used when CONFIG_AMD_WBRF is not set at compile time.
 */

static inline bool
acpi_amd_wbrf_supported_consumer(struct device *dev)
{
	return (false);
}

static inline int
acpi_amd_wbrf_add_remove(struct device *dev, uint8_t action,
    struct wbrf_ranges_in_out *in)
{
	return (-ENODEV);
}

static inline bool
acpi_amd_wbrf_supported_producer(struct device *dev)
{
	return (false);
}

static inline int
amd_wbrf_retrieve_freq_band(struct device *dev, struct wbrf_ranges_in_out *out)
{
	return (-ENODEV);
}

static inline int
amd_wbrf_register_notifier(struct notifier_block *nb)
{
	return (-ENODEV);
}

static inline int
amd_wbrf_unregister_notifier(struct notifier_block *nb)
{
	return (-ENODEV);
}

#endif /* _LINUXKPI_LINUX_ACPI_AMD_WBRF_H_ */
