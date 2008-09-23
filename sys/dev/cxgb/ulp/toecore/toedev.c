
/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/mbuf.h>
#include <sys/proc.h>

#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/route.h>


/*
 * XXX 
 */
#include <cxgb_include.h>


static struct mtx offload_db_lock;
static TAILQ_HEAD(, toedev) offload_dev_list;
static TAILQ_HEAD(, tom_info) offload_module_list;

/*
 * Returns the entry in the given table with the given offload id, or NULL
 * if the id is not found.
 */
static const struct offload_id *
id_find(unsigned int id, const struct offload_id *table)
{
	for ( ; table->id; ++table)
		if (table->id == id)
			return table;
	return NULL;
}

/*
 * Returns true if an offload device is presently attached to an offload module.
 */
static inline int
is_attached(const struct toedev *dev)
{
	return dev->tod_offload_mod != NULL;
}

/*
 * Try to attach a new offload device to an existing TCP offload module that
 * can handle the device's offload id.  Returns 0 if it succeeds.
 *
 * Must be called with the offload_db_lock held.
 */
static int
offload_attach(struct toedev *dev)
{
	struct tom_info *t;

	TAILQ_FOREACH(t, &offload_module_list, entry) {
		const struct offload_id *entry;

		entry = id_find(dev->tod_ttid, t->ti_id_table);
		if (entry && t->ti_attach(dev, entry) == 0) {
			dev->tod_offload_mod = t;
			return 0;
		}
	}
	return (ENOPROTOOPT);
}

/**
 * register_tom - register a TCP Offload Module (TOM)
 * @t: the offload module to register
 *
 * Register a TCP Offload Module (TOM).
 */
int
register_tom(struct tom_info *t)
{
	mtx_lock(&offload_db_lock);
	toedev_registration_count++;
	TAILQ_INSERT_HEAD(&offload_module_list, t, entry);
	mtx_unlock(&offload_db_lock);
	return 0;
}

/**
 * unregister_tom - unregister a TCP Offload Module (TOM)
 * @t: the offload module to register
 *
 * Unregister a TCP Offload Module (TOM).  Note that this does not affect any
 * TOE devices to which the TOM is already attached.
 */
int
unregister_tom(struct tom_info *t)
{
	mtx_lock(&offload_db_lock);
	TAILQ_REMOVE(&offload_module_list, t, entry);
	mtx_unlock(&offload_db_lock);
	return 0;
}

/*
 * Find an offload device by name.  Must be called with offload_db_lock held.
 */
static struct toedev *
__find_offload_dev_by_name(const char *name)
{
	struct toedev *dev;

	TAILQ_FOREACH(dev, &offload_dev_list, entry) {
		if (!strncmp(dev->tod_name, name, TOENAMSIZ))
			return dev;
	}
	return NULL;
}

/*
 * Returns true if an offload device is already registered.
 * Must be called with the offload_db_lock held.
 */
static int
is_registered(const struct toedev *dev)
{
	struct toedev *d;

	TAILQ_FOREACH(d, &offload_dev_list, entry) {
		if (d == dev)
			return 1;
	}
	return 0;
}

/*
 * Finalize the name of an offload device by assigning values to any format
 * strings in its name.
 */
static int
assign_name(struct toedev *dev, const char *name, int limit)
{
	int i;

	for (i = 0; i < limit; ++i) {
		char s[TOENAMSIZ];

		if (snprintf(s, sizeof(s), name, i) >= sizeof(s))
			return -1;                  /* name too long */
		if (!__find_offload_dev_by_name(s)) {
			strcpy(dev->tod_name, s);
			return 0;
		}
	}
	return -1;
}

/**
 * register_toedev - register a TOE device
 * @dev: the device
 * @name: a name template for the device
 *
 * Register a TOE device and try to attach an appropriate TCP offload module
 * to it.  @name is a template that may contain at most one %d format
 * specifier.
 */
int
register_toedev(struct toedev *dev, const char *name)
{
	int ret;
	const char *p;

	/*
	 * Validate the name template.  Only one %d allowed and name must be
	 * a valid filename so it can appear in sysfs.
	 */
	if (!name || !*name || !strcmp(name, ".") || !strcmp(name, "..") ||
	    strchr(name, '/'))
		return EINVAL;

	p = strchr(name, '%');
	if (p && (p[1] != 'd' || strchr(p + 2, '%')))
		return EINVAL;

	mtx_lock(&offload_db_lock);
	if (is_registered(dev)) {  /* device already registered */
		ret = EEXIST;
		goto out;
	}

	if ((ret = assign_name(dev, name, 32)) != 0)
		goto out;

	dev->tod_offload_mod = NULL;
	TAILQ_INSERT_TAIL(&offload_dev_list, dev, entry);
out:
	mtx_unlock(&offload_db_lock);
	return ret;
}

/**
 * unregister_toedev - unregister a TOE device
 * @dev: the device
 *
 * Unregister a TOE device.  The device must not be attached to an offload
 * module.
 */
int
unregister_toedev(struct toedev *dev)
{
	int ret = 0;

	mtx_lock(&offload_db_lock);
	if (!is_registered(dev)) {
		ret = ENODEV;
		goto out;
	}
	if (is_attached(dev)) {
		ret = EBUSY;
		goto out;
	}
	TAILQ_REMOVE(&offload_dev_list, dev, entry);
out:
	mtx_unlock(&offload_db_lock);
	return ret;
}

/**
 * activate_offload - activate an offload device
 * @dev: the device
 *
 * Activate an offload device by locating an appropriate registered offload
 * module.  If no module is found the operation fails and may be retried at
 * a later time.
 */
int
activate_offload(struct toedev *dev)
{
	int ret = 0;

	mtx_lock(&offload_db_lock);
	if (!is_registered(dev))
		ret = ENODEV;
	else if (!is_attached(dev))
		ret = offload_attach(dev);
	mtx_unlock(&offload_db_lock);
	return ret;
}

/**
 * toe_send - send a packet to a TOE device
 * @dev: the device
 * @m: the packet
 *
 * Sends an mbuf to a TOE driver after dealing with any active network taps.
 */
int
toe_send(struct toedev *dev, struct mbuf *m)
{
	int r;

	critical_enter(); /* XXX neccessary? */
	r = dev->tod_send(dev, m);
	critical_exit();
	if (r)
		BPF_MTAP(dev->tod_lldev, m);
	return r;
}

/**
 * toe_receive_mbuf - process n received TOE packets
 * @dev: the toe device
 * @m: an array of offload packets
 * @n: the number of offload packets
 *
 * Process an array of ingress offload packets.  Each packet is forwarded
 * to any active network taps and then passed to the toe device's receive
 * method.  We optimize passing packets to the receive method by passing
 * it the whole array at once except when there are active taps.
 */
int
toe_receive_mbuf(struct toedev *dev, struct mbuf **m, int n)
{
	if (__predict_true(!bpf_peers_present(dev->tod_lldev->if_bpf)))
		return dev->tod_recv(dev, m, n);

	for ( ; n; n--, m++) {
		m[0]->m_pkthdr.rcvif = dev->tod_lldev;
		BPF_MTAP(dev->tod_lldev, m[0]);
		dev->tod_recv(dev, m, 1);
	}
	return 0;
}

static inline int
ifnet_is_offload(const struct ifnet *ifp)
{
	return (ifp->if_flags & IFCAP_TOE);
}

void
toe_arp_update(struct rtentry *rt)
{
	struct ifnet *ifp = rt->rt_ifp;

	if (ifp && ifnet_is_offload(ifp)) {
		struct toedev *tdev = TOEDEV(ifp);

		if (tdev && tdev->tod_arp_update)
			tdev->tod_arp_update(tdev, rt);
	}
}

/**
 * offload_get_phys_egress - find the physical egress device
 * @root_dev: the root device anchoring the search
 * @so: the socket used to determine egress port in bonding mode
 * @context: in bonding mode, indicates a connection set up or failover
 *
 * Given a root network device it returns the physical egress device that is a
 * descendant of the root device.  The root device may be either a physical
 * device, in which case it is the device returned, or a virtual device, such
 * as a VLAN or bonding device.  In case of a bonding device the search
 * considers the decisions of the bonding device given its mode to locate the
 * correct egress device.
 */
struct ifnet *
offload_get_phys_egress(struct ifnet *root_dev, struct socket *so, int context)
{

#if 0
	while (root_dev && ifnet_is_offload(root_dev)) {
		if (root_dev->tod_priv_flags & IFF_802_1Q_VLAN)
			root_dev = VLAN_DEV_INFO(root_dev)->real_dev;
		else if (root_dev->tod_flags & IFF_MASTER)
			root_dev = toe_bond_get_slave(root_dev, sk, context);
		else
			break;
	}
#endif
	return root_dev;
}

static int
toecore_load(module_t mod, int cmd, void *arg)
{
	int err = 0;

	switch (cmd) {
	case MOD_LOAD:
		mtx_init(&offload_db_lock, "toedev lock", NULL, MTX_DEF);
		TAILQ_INIT(&offload_dev_list);
		TAILQ_INIT(&offload_module_list);
		break;
	case MOD_QUIESCE:
		break;
	case MOD_UNLOAD:
		mtx_lock(&offload_db_lock);
		if (!TAILQ_EMPTY(&offload_dev_list) ||
		    !TAILQ_EMPTY(&offload_module_list)) {
			err = EBUSY;
			mtx_unlock(&offload_db_lock);
			break;
		}
		mtx_unlock(&offload_db_lock);
		mtx_destroy(&offload_db_lock);
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		err = EOPNOTSUPP;
		break;
	}

	return (err);
}


static moduledata_t mod_data= {
	"toecore",
	toecore_load,
	0
};

MODULE_VERSION(toecore, 1);
DECLARE_MODULE(toecore, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
