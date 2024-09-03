/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 ARM Ltd
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Arm CoreLink CMN-600 Coherent Mesh Network Driver */

#include <sys/cdefs.h>
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <machine/cmn600_reg.h>

#define	RD4(sc, r)		bus_read_4((sc)->sc_res[0], (r))
#define	RD8(sc, r)		bus_read_8((sc)->sc_res[0], (r))
#define	WR4(sc, r, v)		bus_write_4((sc)->sc_res[0], (r), (v))
#define	WR8(sc, r, v)		bus_write_8((sc)->sc_res[0], (r), (v))
#define	FLD(v, n)		(((v) & n ## _MASK) >> n ## _SHIFT)

static char *cmn600_ids[] = {
	"ARMHC600",
	NULL
};

static struct resource_spec cmn600_res_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE | RF_UNMAPPED | RF_OPTIONAL },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

struct cmn600_node;

typedef uint64_t (*nd_read_8_t)(struct cmn600_node *, uint32_t);
typedef uint32_t (*nd_read_4_t)(struct cmn600_node *, uint32_t);
typedef void (*nd_write_8_t)(struct cmn600_node *, uint32_t, uint64_t);
typedef void (*nd_write_4_t)(struct cmn600_node *, uint32_t, uint32_t);

struct cmn600_node {
	struct cmn600_softc	*sc;
	off_t			 nd_offset;
	int			 nd_type;
	uint16_t		 nd_id;
	uint16_t		 nd_logical_id;
	uint8_t			 nd_x, nd_y, nd_port, nd_sub;
	uint16_t		 nd_child_count;
	uint32_t		 nd_paired;
	struct cmn600_node	*nd_parent;
	nd_read_8_t		 nd_read8;
	nd_read_4_t		 nd_read4;
	nd_write_8_t		 nd_write8;
	nd_write_4_t		 nd_write4;
	struct cmn600_node	**nd_children;
};

struct cmn600_softc {
	device_t	 sc_dev;
	int		 sc_unit;
	int		 sc_domain;
	int		 sc_longid;
	int		 sc_mesh_x;
	int		 sc_mesh_y;
	struct resource *sc_res[3];
	void		*sc_ih;
	int		 sc_r2;
	int		 sc_rev;
	struct cmn600_node *sc_rootnode;
	struct cmn600_node *sc_dtcnode;
	struct cmn600_node *sc_dvmnode;
	struct cmn600_node *sc_xpnodes[64];
	int (*sc_pmu_ih)(struct trapframe *tf, int unit, int i);
};

static struct cmn600_pmc cmn600_pmcs[CMN600_UNIT_MAX];
static int cmn600_npmcs = 0;

static int cmn600_acpi_detach(device_t dev);
static int cmn600_intr(void *arg);

static void
cmn600_pmc_register(int unit, void *arg, int domain)
{

	if (unit >= CMN600_UNIT_MAX) {
		/* TODO */
		return;
	}

	cmn600_pmcs[unit].arg = arg;
	cmn600_pmcs[unit].domain = domain;
	cmn600_npmcs++;
}

static void
cmn600_pmc_unregister(int unit)
{

	cmn600_pmcs[unit].arg = NULL;
	cmn600_npmcs--;
}

int
cmn600_pmc_nunits(void)
{

	return (cmn600_npmcs);
}

int
cmn600_pmc_getunit(int unit, void **arg, int *domain)
{

	if (unit >= cmn600_npmcs)
		return (EINVAL);
	if (cmn600_pmcs[unit].arg == NULL)
		return (EINVAL);
	*arg = cmn600_pmcs[unit].arg;
	*domain = cmn600_pmcs[unit].domain;
	return (0);
}

int
pmu_cmn600_rev(void *arg)
{
	struct cmn600_softc *sc;

	sc = (struct cmn600_softc *)arg;
	switch (sc->sc_rev) {
	case 0x0:
		return (0x100);
	case 0x1:
		return (0x101);
	case 0x2:
		return (0x102);
	case 0x3:
		return (0x103);
	case 0x4:
		return (0x200);
	case 0x5:
		return (0x300);
	case 0x6:
		return (0x301);
	}
	return (0x302); /* Unknown revision. */
}

static uint64_t
cmn600_node_read8(struct cmn600_node *nd, uint32_t reg)
{

	return (RD8(nd->sc, nd->nd_offset + reg));
}

static void
cmn600_node_write8(struct cmn600_node *nd, uint32_t reg, uint64_t val)
{

	WR8(nd->sc, nd->nd_offset + reg, val);
}

static uint32_t
cmn600_node_read4(struct cmn600_node *nd, uint32_t reg)
{

	return (RD4(nd->sc, nd->nd_offset + reg));
}

static void
cmn600_node_write4(struct cmn600_node *nd, uint32_t reg, uint32_t val)
{

	WR4(nd->sc, nd->nd_offset + reg, val);
}

static const char *
cmn600_node_type_str(int type)
{

#define	NAME_OF(t, n)	case NODE_TYPE_ ## t: return n
	switch (type) {
	NAME_OF(INVALID, "<invalid node>");
	NAME_OF(DVM, "DVM");
	NAME_OF(CFG, "CFG");
	NAME_OF(DTC, "DTC");
	NAME_OF(HN_I, "HN-I");
	NAME_OF(HN_F, "HN-F");
	NAME_OF(XP, "XP");
	NAME_OF(SBSX, "SBSX");
	NAME_OF(RN_I, "RN-I");
	NAME_OF(RN_D, "RN-D");
	NAME_OF(RN_SAM, "RN-SAM");
	NAME_OF(CXRA, "CXRA");
	NAME_OF(CXHA, "CXHA");
	NAME_OF(CXLA, "CXLA");
	default:
		return "<unknown node>";
	}
#undef	NAME_OF
}

static const char *
cmn600_xpport_dev_type_str(uint8_t type)
{

#define	NAME_OF(t, n)	case POR_MXP_PX_INFO_DEV_TYPE_ ## t: return n
	switch (type) {
	NAME_OF(RN_I, "RN-I");
	NAME_OF(RN_D, "RN-D");
	NAME_OF(RN_F_CHIB, "RN-F CHIB");
	NAME_OF(RN_F_CHIB_ESAM, "RN-F CHIB ESAM");
	NAME_OF(RN_F_CHIA, "RN-F CHIA");
	NAME_OF(RN_F_CHIA_ESAM, "RN-F CHIA ESAM");
	NAME_OF(HN_T, "HN-T");
	NAME_OF(HN_I, "HN-I");
	NAME_OF(HN_D, "HN-D");
	NAME_OF(SN_F, "SN-F");
	NAME_OF(SBSX, "SBSX");
	NAME_OF(HN_F, "HN-F");
	NAME_OF(CXHA, "CXHA");
	NAME_OF(CXRA, "CXRA");
	NAME_OF(CXRH, "CXRH");
	default:
		return "<unknown>";
	}
#undef	NAME_OF
}

static void
cmn600_dump_node(struct cmn600_node *node, int lvl)
{
	int i;

	for (i = 0; i < lvl; i++) printf("    ");
	printf("%s [%dx%d:%d:%d] id: 0x%x @0x%lx Logical Id: 0x%x",
	    cmn600_node_type_str(node->nd_type), node->nd_x, node->nd_y,
	    node->nd_port, node->nd_sub, node->nd_id, node->nd_offset,
	    node->nd_logical_id);
	if (node->nd_child_count > 0)
		printf(", Children: %d", node->nd_child_count);
	printf("\n");
	if (node->nd_type == NODE_TYPE_XP)
		printf("\tPort 0: %s\n\tPort 1: %s\n",
		    cmn600_xpport_dev_type_str(node->nd_read4(node,
			POR_MXP_P0_INFO) & 0x1f),
		    cmn600_xpport_dev_type_str(node->nd_read4(node,
			POR_MXP_P1_INFO) & 0x1f));
}

static void
cmn600_dump_node_recursive(struct cmn600_node *node, int lvl)
{
	int i;

	cmn600_dump_node(node, lvl);
	for (i = 0; i < node->nd_child_count; i++) {
		cmn600_dump_node_recursive(node->nd_children[i], lvl + 1);
	}
}

static void
cmn600_dump_nodes_tree(struct cmn600_softc *sc)
{

	device_printf(sc->sc_dev, " nodes:\n");
	cmn600_dump_node_recursive(sc->sc_rootnode, 0);
}

static int
cmn600_sysctl_dump_nodes(SYSCTL_HANDLER_ARGS)
{
	struct cmn600_softc *sc;
	uint32_t val;
	int err;

	sc = (struct cmn600_softc *)arg1;
	val = 0;
	err = sysctl_handle_int(oidp, &val, 0, req);

	if (err)
		return (err);

	if (val != 0)
		cmn600_dump_nodes_tree(sc);

	return (0);
}

static struct cmn600_node *
cmn600_create_node(struct cmn600_softc *sc, off_t node_offset,
    struct cmn600_node *parent, int lvl)
{
	struct cmn600_node *node;
	off_t child_offset;
	uint64_t val;
	int i;

	node = malloc(sizeof(struct cmn600_node), M_DEVBUF, M_WAITOK);
	node->sc = sc;
	node->nd_offset = node_offset;
	node->nd_parent = parent;
	node->nd_read4 = cmn600_node_read4;
	node->nd_read8 = cmn600_node_read8;
	node->nd_write4 = cmn600_node_write4;
	node->nd_write8 = cmn600_node_write8;

	val = node->nd_read8(node, POR_CFGM_NODE_INFO);
	node->nd_type = FLD(val, POR_CFGM_NODE_INFO_NODE_TYPE);
	node->nd_id = FLD(val, POR_CFGM_NODE_INFO_NODE_ID);
	node->nd_logical_id = FLD(val, POR_CFGM_NODE_INFO_LOGICAL_ID);

	val = node->nd_read8(node, POR_CFGM_CHILD_INFO);
	node->nd_child_count = FLD(val, POR_CFGM_CHILD_INFO_CHILD_COUNT);
	child_offset = FLD(val, POR_CFGM_CHILD_INFO_CHILD_PTR_OFFSET);

	if (parent == NULL) {
		/* Find XP node with Id 8. It have to be last in a row. */
		for (i = 0; i < node->nd_child_count; i++) {
			val = node->nd_read8(node, child_offset + (i * 8));
			val &= POR_CFGM_CHILD_POINTER_BASE_MASK;
			val = RD8(sc, val + POR_CFGM_NODE_INFO);

			if (FLD(val, POR_CFGM_NODE_INFO_NODE_ID) != 8)
				continue;

			sc->sc_mesh_x = FLD(val, POR_CFGM_NODE_INFO_LOGICAL_ID);
			sc->sc_mesh_y = node->nd_child_count / sc->sc_mesh_x;
			if (bootverbose)
				printf("Mesh width X/Y %d/%d\n", sc->sc_mesh_x,
				    sc->sc_mesh_y);

			if ((sc->sc_mesh_x > 4) || (sc->sc_mesh_y > 4))
				sc->sc_longid = 1;
			break;
		}

		val = node->nd_read8(node, POR_INFO_GLOBAL);
		sc->sc_r2 = (val & POR_INFO_GLOBAL_R2_ENABLE) ? 1 : 0;
		val = node->nd_read4(node, POR_CFGM_PERIPH_ID_2_PERIPH_ID_3);
		sc->sc_rev = FLD(val, POR_CFGM_PERIPH_ID_2_REV);
		if (bootverbose)
			printf("  Rev: %d, R2_ENABLE = %s\n", sc->sc_rev,
			    sc->sc_r2 ? "true" : "false");
	}
	node->nd_sub = FLD(node->nd_id, NODE_ID_SUB);
	node->nd_port = FLD(node->nd_id, NODE_ID_PORT);
	node->nd_paired = 0;
	if (sc->sc_longid == 1) {
		node->nd_x = FLD(node->nd_id, NODE_ID_X3B);
		node->nd_y = FLD(node->nd_id, NODE_ID_Y3B);
	} else {
		node->nd_x = FLD(node->nd_id, NODE_ID_X2B);
		node->nd_y = FLD(node->nd_id, NODE_ID_Y2B);
	}

	if (bootverbose) {
		cmn600_dump_node(node, lvl);
	}

	node->nd_children = (struct cmn600_node **)mallocarray(
	    node->nd_child_count, sizeof(struct cmn600_node *), M_DEVBUF,
	    M_WAITOK);
	for (i = 0; i < node->nd_child_count; i++) {
		val = node->nd_read8(node, child_offset + (i * 8));
		node->nd_children[i] = cmn600_create_node(sc, val &
		    POR_CFGM_CHILD_POINTER_BASE_MASK, node, lvl + 1);
	}
	switch (node->nd_type) {
	case NODE_TYPE_DTC:
		sc->sc_dtcnode = node;
		break;
	case NODE_TYPE_DVM:
		sc->sc_dvmnode = node;
		break;
	case NODE_TYPE_XP:
		sc->sc_xpnodes[node->nd_id >> NODE_ID_X2B_SHIFT] = node;
		break;
	default:
		break;
	}
	return (node);
}

static void
cmn600_destroy_node(struct cmn600_node *node)
{
	int i;

	for (i = 0; i < node->nd_child_count; i++) {
		if (node->nd_children[i] == NULL)
			continue;
		cmn600_destroy_node(node->nd_children[i]);
	}
	free(node->nd_children, M_DEVBUF);
	free(node, M_DEVBUF);
}

static int
cmn600_find_node(struct cmn600_softc *sc, int node_id, int type,
    struct cmn600_node **node)
{
	struct cmn600_node *xp, *child;
	uint8_t xp_xy;
	int i;

	switch (type) {
	case NODE_TYPE_INVALID:
		return (ENXIO);
	case NODE_TYPE_CFG:
		*node = sc->sc_rootnode;
		return (0);
	case NODE_TYPE_DTC:
		*node = sc->sc_dtcnode;
		return (0);
	case NODE_TYPE_DVM:
		*node = sc->sc_dvmnode;
		return (0);
	default:
		break;
	}

	xp_xy = node_id >> NODE_ID_X2B_SHIFT;
	if (xp_xy >= 64)
		return (ENXIO);
	if (sc->sc_xpnodes[xp_xy] == NULL)
		return (ENOENT);

	switch (type) {
	case NODE_TYPE_XP:
		*node = sc->sc_xpnodes[xp_xy];
		return (0);
	default:
		xp = sc->sc_xpnodes[xp_xy];
		for (i = 0; i < xp->nd_child_count; i++) {
			child = xp->nd_children[i];
			if (child->nd_id == node_id && child->nd_type == type) {
				*node = child;
				return (0);
			}
		}
	}
	return (ENOENT);
}

int
pmu_cmn600_alloc_localpmc(void *arg, int nodeid, int node_type, int *counter)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	uint32_t new, old;
	int i, ret;

	sc = (struct cmn600_softc *)arg;
	switch (node_type) {
	case NODE_TYPE_CXLA:
		break;
	default:
		node_type = NODE_TYPE_XP;
		/* Parent XP node has always zero port and device bits. */
		nodeid &= ~0x07;
	}
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (ret);
	for (i = 0; i < 4; i++) {
		new = old = node->nd_paired;
		if (old == 0xf)
			return (EBUSY);
		if ((old & (1 << i)) != 0)
			continue;
		new |= 1 << i;
		if (atomic_cmpset_32(&node->nd_paired, old, new) != 0)
			break;
	}
	*counter = i;
	return (0);
}

int
pmu_cmn600_free_localpmc(void *arg, int nodeid, int node_type, int counter)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	uint32_t new, old;
	int ret;

	sc = (struct cmn600_softc *)arg;
	switch (node_type) {
	case NODE_TYPE_CXLA:
		break;
	default:
		node_type = NODE_TYPE_XP;
	}
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (ret);

	do {
		new = old = node->nd_paired;
		new &= ~(1 << counter);
	} while (atomic_cmpset_32(&node->nd_paired, old, new) == 0);
	return (0);
}

uint32_t
pmu_cmn600_rd4(void *arg, int nodeid, int node_type, off_t reg)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	int ret;

	sc = (struct cmn600_softc *)arg;
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (UINT32_MAX);
	return (cmn600_node_read4(node, reg));
}

int
pmu_cmn600_wr4(void *arg, int nodeid, int node_type, off_t reg, uint32_t val)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	int ret;

	sc = (struct cmn600_softc *)arg;
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (ret);
	cmn600_node_write4(node, reg, val);
	return (0);
}

uint64_t
pmu_cmn600_rd8(void *arg, int nodeid, int node_type, off_t reg)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	int ret;

	sc = (struct cmn600_softc *)arg;
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (UINT64_MAX);
	return (cmn600_node_read8(node, reg));
}

int
pmu_cmn600_wr8(void *arg, int nodeid, int node_type, off_t reg, uint64_t val)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	int ret;

	sc = (struct cmn600_softc *)arg;
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (ret);
	cmn600_node_write8(node, reg, val);
	return (0);
}

int
pmu_cmn600_set8(void *arg, int nodeid, int node_type, off_t reg, uint64_t val)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	int ret;

	sc = (struct cmn600_softc *)arg;
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (ret);
	cmn600_node_write8(node, reg, cmn600_node_read8(node, reg) | val);
	return (0);
}

int
pmu_cmn600_clr8(void *arg, int nodeid, int node_type, off_t reg, uint64_t val)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	int ret;

	sc = (struct cmn600_softc *)arg;
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (ret);
	cmn600_node_write8(node, reg, cmn600_node_read8(node, reg) & ~val);
	return (0);
}

int
pmu_cmn600_md8(void *arg, int nodeid, int node_type, off_t reg, uint64_t mask,
    uint64_t val)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	int ret;

	sc = (struct cmn600_softc *)arg;
	ret = cmn600_find_node(sc, nodeid, node_type, &node);
	if (ret != 0)
		return (ret);
	cmn600_node_write8(node, reg, (cmn600_node_read8(node, reg) & ~mask) |
	    val);
	return (0);
}

static int
cmn600_acpi_probe(device_t dev)
{
	int err;

	err = ACPI_ID_PROBE(device_get_parent(dev), dev, cmn600_ids, NULL);
	if (err <= 0)
		device_set_desc(dev, "Arm CoreLink CMN-600 Coherent Mesh Network");

	return (err);
}

static int
cmn600_acpi_attach(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *child;
	struct cmn600_softc *sc;
	int cpu, domain, i, u;
	const char *dname;
	rman_res_t count, periph_base, rootnode_base;
	struct cmn600_node *node;

	dname = device_get_name(dev);
	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	u = device_get_unit(dev);
	sc->sc_unit = u;
	domain = 0;

	if ((resource_int_value(dname, u, "domain", &domain) == 0 ||
	    bus_get_domain(dev, &domain) == 0) && domain < MAXMEMDOM) {
		sc->sc_domain = domain;
	}
	if (domain == -1) /* NUMA not supported. Use single domain. */
		domain = 0;
	sc->sc_domain = domain;
	device_printf(dev, "domain=%d\n", sc->sc_domain);

	cpu = CPU_FFS(&cpuset_domain[domain]) - 1;

	i = bus_alloc_resources(dev, cmn600_res_spec, sc->sc_res);
	if (i != 0) {
		device_printf(dev, "cannot allocate resources for device (%d)\n",
		    i);
		return (i);
	}

	bus_get_resource(dev, cmn600_res_spec[0].type, cmn600_res_spec[0].rid,
	    &periph_base, &count);
	bus_get_resource(dev, cmn600_res_spec[1].type, cmn600_res_spec[1].rid,
	    &rootnode_base, &count);
	rootnode_base -= periph_base;
	if (bootverbose)
		printf("ROOTNODE at %lx x %lx\n", rootnode_base, count);

	sc->sc_rootnode = cmn600_create_node(sc, rootnode_base, NULL, 0);
	ctx = device_get_sysctl_ctx(sc->sc_dev);

	child = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->sc_dev));
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "dump_nodes", CTLTYPE_INT |
	    CTLFLAG_RW | CTLFLAG_NEEDGIANT, sc, 0, cmn600_sysctl_dump_nodes,
	    "U", "Dump CMN-600 nodes tree");

	node = sc->sc_dtcnode;
	if (node == NULL)
		return (ENXIO);

	cmn600_pmc_register(sc->sc_unit, (void *)sc, domain);

	node->nd_write8(node, POR_DT_PMCR, 0);
	node->nd_write8(node, POR_DT_PMOVSR_CLR, POR_DT_PMOVSR_ALL);
	node->nd_write8(node, POR_DT_PMCR, POR_DT_PMCR_OVFL_INTR_EN);
	node->nd_write8(node, POR_DT_DTC_CTL, POR_DT_DTC_CTL_DT_EN);

	if (bus_setup_intr(dev, sc->sc_res[2], INTR_TYPE_MISC | INTR_MPSAFE,
	    cmn600_intr, NULL, sc, &sc->sc_ih)) {
		bus_release_resources(dev, cmn600_res_spec, sc->sc_res);
		device_printf(dev, "cannot setup interrupt handler\n");
		cmn600_acpi_detach(dev);
		return (ENXIO);
	}
	if (bus_bind_intr(dev, sc->sc_res[2], cpu)) {
		bus_teardown_intr(dev, sc->sc_res[2], sc->sc_ih);
		bus_release_resources(dev, cmn600_res_spec, sc->sc_res);
		device_printf(dev, "cannot setup interrupt handler\n");
		cmn600_acpi_detach(dev);
		return (ENXIO);
	}
	return (0);
}

static int
cmn600_acpi_detach(device_t dev)
{
	struct cmn600_softc *sc;
	struct cmn600_node *node;

	sc = device_get_softc(dev);
	if (sc->sc_res[2] != NULL) {
		bus_teardown_intr(dev, sc->sc_res[2], sc->sc_ih);
	}

	node = sc->sc_dtcnode;
	node->nd_write4(node, POR_DT_DTC_CTL,
	    node->nd_read4(node, POR_DT_DTC_CTL) & ~POR_DT_DTC_CTL_DT_EN);
	node->nd_write8(node, POR_DT_PMOVSR_CLR, POR_DT_PMOVSR_ALL);

	cmn600_pmc_unregister(sc->sc_unit);
	cmn600_destroy_node(sc->sc_rootnode);
	bus_release_resources(dev, cmn600_res_spec, sc->sc_res);

	return (0);
}

int
cmn600_pmu_intr_cb(void *arg, int (*handler)(struct trapframe *tf, int unit,
    int i))
{
	struct cmn600_softc *sc;

	sc = (struct cmn600_softc *) arg;
	sc->sc_pmu_ih = handler;
	return (0);
}

static int
cmn600_intr(void *arg)
{
	struct cmn600_node *node;
	struct cmn600_softc *sc;
	struct trapframe *tf;
	uint64_t mask, ready, val;
	int i;
	
	tf = PCPU_GET(curthread)->td_intr_frame;
	sc = (struct cmn600_softc *) arg;
	node = sc->sc_dtcnode;
	val = node->nd_read8(node, POR_DT_PMOVSR);
	if (val & POR_DT_PMOVSR_CYCLE_COUNTER)
		node->nd_write8(node, POR_DT_PMOVSR_CLR,
		    POR_DT_PMOVSR_CYCLE_COUNTER);
	if (val & POR_DT_PMOVSR_EVENT_COUNTERS) {
		for (ready = 0, i = 0; i < 8; i++) {
			mask = 1 << i;
			if ((val & mask) == 0)
				continue;
			if (sc->sc_pmu_ih != NULL)
				sc->sc_pmu_ih(tf, sc->sc_unit, i);
			ready |= mask;

		}
		node->nd_write8(node, POR_DT_PMOVSR_CLR, ready);
	}

	return (FILTER_HANDLED);
}

static device_method_t cmn600_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			cmn600_acpi_probe),
	DEVMETHOD(device_attach,		cmn600_acpi_attach),
	DEVMETHOD(device_detach,		cmn600_acpi_detach),

	/* End */
	DEVMETHOD_END
};

static driver_t cmn600_acpi_driver = {
	"cmn600",
	cmn600_acpi_methods,
	sizeof(struct cmn600_softc),
};

DRIVER_MODULE(cmn600, acpi, cmn600_acpi_driver, 0, 0);
MODULE_VERSION(cmn600, 1);
