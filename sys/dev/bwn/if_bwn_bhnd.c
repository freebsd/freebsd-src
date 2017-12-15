/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2016 Landon Fuller <landonf@FreeBSD.org>
 * Copyright (c) 2017 The FreeBSD Foundation
 * All rights reserved.
 * 
 * Portions of this software were developed by Landon Fuller
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bwn.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_phy.h>
#include <net80211/ieee80211_ratectl.h>

#include <dev/bhnd/bhnd.h>
#include <dev/bhnd/bhnd_ids.h>

#include "if_bwnvar.h"

/* Supported device identifiers */
#define	BWN_DEV(_hwrev)	{{					\
	BHND_MATCH_CORE(BHND_MFGID_BCM, BHND_COREID_D11),	\
	BHND_MATCH_CORE_REV(_hwrev),				\
}}

static const struct bhnd_device bwn_devices[] = {
	BWN_DEV(HWREV_RANGE(5, 16)),
	BWN_DEV(HWREV_EQ(23)),

	BHND_DEVICE_END
};

static int
bwn_bhnd_probe(device_t dev)
{
	const struct bhnd_device *id;

	id = bhnd_device_lookup(dev, bwn_devices, sizeof(bwn_devices[0]));
	if (id == NULL)
		return (ENXIO);

	bhnd_set_default_core_desc(dev);
	return (BUS_PROBE_DEFAULT);
}

static device_method_t bwn_bhnd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bwn_bhnd_probe),

	DEVMETHOD_END
};

static devclass_t bwn_devclass;

DEFINE_CLASS_1(bwn, bwn_bhnd_driver, bwn_bhnd_methods, sizeof(struct bwn_softc),
    bwn_driver);

DRIVER_MODULE(bwn_bhnd, bhnd, bwn_bhnd_driver, bwn_devclass, 0, 0);
MODULE_DEPEND(bwn_bhnd, bhnd, 1, 1, 1);
MODULE_VERSION(bwn_bhnd, 1);
