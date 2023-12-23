/*-
 * Copyright (c) 2022 The FreeBSD Foundation
 *
 * This software was developed by Andrew Turner under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This manages all hwresets for a device and asserts/deasserts them in
 * an undefined order.
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/malloc.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include <dev/extres/hwreset/hwreset.h>

MALLOC_DECLARE(M_HWRESET);

struct hwreset_array {
	hwreset_t *rst_array;
	int	count;
};

int
hwreset_array_assert(hwreset_array_t rsts)
{
	int i, rv;

	for (i = 0; i < rsts->count; i++) {
		rv = hwreset_assert(rsts->rst_array[i]);
		if (rv != 0)
			return (rv);
	}

	return (0);
}

int
hwreset_array_deassert(hwreset_array_t rsts)
{
	int i, rv;

	for (i = 0; i < rsts->count; i++) {
		rv = hwreset_deassert(rsts->rst_array[i]);
		if (rv != 0)
			return (rv);
	}

	return (0);
}

void
hwreset_array_release(hwreset_array_t rsts)
{
	int i;

	for (i = 0; i < rsts->count; i++) {
		hwreset_release(rsts->rst_array[i]);
	}
	free(rsts->rst_array, M_HWRESET);
	free(rsts, M_HWRESET);
}

#ifdef FDT
int
hwreset_array_get_ofw(device_t consumer_dev, phandle_t cnode,
    hwreset_array_t *rsts)
{
	hwreset_array_t resets;
	int count, i, rv;

	if (cnode <= 0)
		cnode = ofw_bus_get_node(consumer_dev);
	if (cnode <= 0) {
		device_printf(consumer_dev,
		    "%s called on not ofw based device\n", __func__);
		return (ENXIO);
	}

	rv = ofw_bus_parse_xref_list_get_length(cnode, "resets", "#reset-cells",
	    &count);
	if (rv != 0)
		return (rv);

	resets = malloc(sizeof(struct hwreset_array), M_HWRESET,
	    M_WAITOK | M_ZERO);
	resets->rst_array = mallocarray(count, sizeof(hwreset_t), M_HWRESET,
	    M_WAITOK | M_ZERO);

	for (i = 0; i < count; i++) {
		rv = hwreset_get_by_ofw_idx(consumer_dev, cnode, i,
		    &resets->rst_array[i]);
		if (rv != 0)
			break;
	}

	if (rv != 0) {
		count = i;
		for (i = 0; i < count; i++) {
			hwreset_release(resets->rst_array[i]);
		}
		free(resets->rst_array, M_HWRESET);
		free(resets, M_HWRESET);
	} else {
		resets->count = count;
		*rsts = resets;
	}
	return (rv);
}
#endif
