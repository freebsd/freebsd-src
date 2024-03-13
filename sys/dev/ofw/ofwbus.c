/*-
 * Copyright 1998 Massachusetts Institute of Technology
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.
 * Copyright 2006 by Marius Strobl <marius@FreeBSD.org>.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	from: FreeBSD: src/sys/i386/i386/nexus.c,v 1.43 2001/02/09
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>
#include <dev/fdt/simplebus.h>

#include <machine/bus.h>
#include <machine/resource.h>

/*
 * The ofwbus (which is a pseudo-bus actually) iterates over the nodes that
 * hang from the Open Firmware root node and adds them as devices to this bus
 * (except some special nodes which are excluded) so that drivers can be
 * attached to them. There should be only one ofwbus in the system, added
 * directly as a child of nexus0.
 */

static device_probe_t ofwbus_probe;
static device_attach_t ofwbus_attach;
static bus_alloc_resource_t ofwbus_alloc_resource;
static bus_release_resource_t ofwbus_release_resource;

static device_method_t ofwbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofwbus_probe),
	DEVMETHOD(device_attach,	ofwbus_attach),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,	ofwbus_alloc_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_adjust_resource),
	DEVMETHOD(bus_release_resource,	ofwbus_release_resource),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ofwbus, ofwbus_driver, ofwbus_methods,
    sizeof(struct simplebus_softc), simplebus_driver);
EARLY_DRIVER_MODULE(ofwbus, nexus, ofwbus_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ofwbus, 1);

static int
ofwbus_probe(device_t dev)
{

	if (OF_peer(0) == 0)
		return (ENXIO);

	/* Only one instance of ofwbus. */
	if (device_get_unit(dev) != 0)
		panic("ofwbus added with non-zero unit number: %d\n",
		    device_get_unit(dev));

	device_set_desc(dev, "Open Firmware Device Tree");
	return (BUS_PROBE_NOWILDCARD);
}

static int
ofwbus_attach(device_t dev)
{
	phandle_t node;

	node = OF_peer(0);

	/*
	 * If no Open Firmware, bail early
	 */
	if (node == -1)
		return (ENXIO);

	/*
	 * ofwbus bus starts on unamed node in FDT, so we cannot make
	 * ofw_bus_devinfo from it. Pass node to simplebus_init directly.
	 */
	simplebus_init(dev, node);

	/*
	 * Allow devices to identify.
	 */
	bus_generic_probe(dev);

	/*
	 * Now walk the OFW tree and attach top-level devices.
	 */
	for (node = OF_child(node); node > 0; node = OF_peer(node))
		simplebus_add_device(dev, node, 0, NULL, -1, NULL);

	return (bus_generic_attach(dev));
}

static struct resource *
ofwbus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource *rv;
	struct resource_list_entry *rle;
	bool isdefault, passthrough;

	isdefault = RMAN_IS_DEFAULT_RANGE(start, end);
	passthrough = (device_get_parent(child) != bus);
	rle = NULL;
	if (!passthrough && isdefault) {
		rle = resource_list_find(BUS_GET_RESOURCE_LIST(bus, child),
		    type, *rid);
		if (rle == NULL) {
			if (bootverbose)
				device_printf(bus, "no default resources for "
				    "rid = %d, type = %d\n", *rid, type);
			return (NULL);
		}
		start = rle->start;
		count = ummax(count, rle->count);
		end = ummax(rle->end, start + count - 1);
	}

	/* Let nexus handle the allocation. */
	rv = bus_generic_alloc_resource(bus, child, type, rid, start, end,
	    count, flags);
	if (rv == NULL)
		return (NULL);

	if (!passthrough && rle != NULL) {
		rle->res = rv;
		rle->start = rman_get_start(rv);
		rle->end = rman_get_end(rv);
		rle->count = rle->end - rle->start + 1;
	}

	return (rv);
}

static int
ofwbus_release_resource(device_t bus, device_t child, struct resource *r)
{
	struct resource_list_entry *rle;
	bool passthrough;

	passthrough = (device_get_parent(child) != bus);
	if (!passthrough) {
		/* Clean resource list entry */
		rle = resource_list_find(BUS_GET_RESOURCE_LIST(bus, child),
		    rman_get_type(r), rman_get_rid(r));
		if (rle != NULL)
			rle->res = NULL;
	}

	/* Let nexus handle the release. */
	return (bus_generic_release_resource(bus, child, r));
}
