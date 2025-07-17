/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2018 Marvell International Ltd.
 *
 * Author: Jayachandran C Nair <jchandra@freebsd.org>
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

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/intr.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/actables.h>

#include <dev/acpica/acpivar.h>

/*
 * Track next XREF available for ITS groups.
 */
static u_int acpi_its_xref = ACPI_MSI_XREF;

/*
 * Some types of IORT nodes have a set of mappings.  Each of them map
 * a range of device IDs [base..end] from the current node to another
 * node. The corresponding device IDs on destination node starts at
 * outbase.
 */
struct iort_map_entry {
	u_int			base;
	u_int			end;
	u_int			outbase;
	u_int			flags;
	u_int			out_node_offset;
	struct iort_node	*out_node;
};

/*
 * The ITS group node does not have any outgoing mappings. It has a
 * of a list of GIC ITS blocks which can handle the device ID. We
 * will store the PIC XREF used by the block and the blocks proximity
 * data here, so that it can be retrieved together.
 */
struct iort_its_entry {
	u_int			its_id;
	u_int			xref;
	int			pxm;
};

struct iort_named_component
{
	UINT32                  NodeFlags;
	UINT64                  MemoryProperties;
	UINT8                   MemoryAddressLimit;
	char                    DeviceName[32]; /* Path of namespace object */
};

/*
 * IORT node. Each node has some device specific data depending on the
 * type of the node. The node can also have a set of mappings, OR in
 * case of ITS group nodes a set of ITS entries.
 * The nodes are kept in a TAILQ by type.
 */
struct iort_node {
	TAILQ_ENTRY(iort_node)	next;		/* next entry with same type */
	enum AcpiIortNodeType	type;		/* ACPI type */
	u_int			node_offset;	/* offset in IORT - node ID */
	u_int			nentries;	/* items in array below */
	u_int			usecount;	/* for bookkeeping */
	u_int			revision;	/* node revision */
	union {
		struct iort_map_entry	*mappings;	/* node mappings  */
		struct iort_its_entry	*its;		/* ITS IDs array */
	} entries;
	union {
		ACPI_IORT_ROOT_COMPLEX		pci_rc;	/* PCI root complex */
		ACPI_IORT_SMMU			smmu;
		ACPI_IORT_SMMU_V3		smmu_v3;
		struct iort_named_component	named_comp;
	} data;
};

/* Lists for each of the types. */
static TAILQ_HEAD(, iort_node) pci_nodes = TAILQ_HEAD_INITIALIZER(pci_nodes);
static TAILQ_HEAD(, iort_node) smmu_nodes = TAILQ_HEAD_INITIALIZER(smmu_nodes);
static TAILQ_HEAD(, iort_node) its_groups = TAILQ_HEAD_INITIALIZER(its_groups);
static TAILQ_HEAD(, iort_node) named_nodes = TAILQ_HEAD_INITIALIZER(named_nodes);

static int
iort_entry_get_id_mapping_index(struct iort_node *node)
{

	switch(node->type) {
	case ACPI_IORT_NODE_SMMU_V3:
		/* The ID mapping field was added in version 1 */
		if (node->revision < 1)
			return (-1);

		/*
		 * If all the control interrupts are GISCV based the ID
		 * mapping field is ignored.
		 */
		if (node->data.smmu_v3.EventGsiv != 0 &&
		    node->data.smmu_v3.PriGsiv != 0 &&
		    node->data.smmu_v3.GerrGsiv != 0 &&
		    node->data.smmu_v3.SyncGsiv != 0)
			return (-1);

		if (node->data.smmu_v3.IdMappingIndex >= node->nentries)
			return (-1);

		return (node->data.smmu_v3.IdMappingIndex);
	case ACPI_IORT_NODE_PMCG:
		return (0);
	default:
		break;
	}

	return (-1);
}

/*
 * Lookup an ID in the mappings array. If successful, map the input ID
 * to the output ID and return the output node found.
 */
static struct iort_node *
iort_entry_lookup(struct iort_node *node, u_int id, u_int *outid)
{
	struct iort_map_entry *entry;
	int i, id_map;

	id_map = iort_entry_get_id_mapping_index(node);
	entry = node->entries.mappings;
	for (i = 0; i < node->nentries; i++, entry++) {
		if (i == id_map)
			continue;
		if (entry->base <= id && id <= entry->end)
			break;
	}
	if (i == node->nentries)
		return (NULL);
	if ((entry->flags & ACPI_IORT_ID_SINGLE_MAPPING) == 0)
		*outid = entry->outbase + (id - entry->base);
	else
		*outid = entry->outbase;
	return (entry->out_node);
}

/*
 * Perform an additional lookup in case of SMMU node and ITS outtype.
 */
static struct iort_node *
iort_smmu_trymap(struct iort_node *node, u_int outtype, u_int *outid)
{
	/* Original node can be not found. */
	if (!node)
		return (NULL);

	/* Node can be SMMU or ITS. If SMMU, we need another lookup. */
	if (outtype == ACPI_IORT_NODE_ITS_GROUP &&
	    (node->type == ACPI_IORT_NODE_SMMU_V3 ||
	     node->type == ACPI_IORT_NODE_SMMU)) {
		node = iort_entry_lookup(node, *outid, outid);
		if (node == NULL)
			return (NULL);
	}

	KASSERT(node->type == outtype, ("mapping fail"));
	return (node);
}

/*
 * Map a PCI RID to a SMMU node or an ITS node, based on outtype.
 */
static struct iort_node *
iort_pci_rc_map(u_int seg, u_int rid, u_int outtype, u_int *outid)
{
	struct iort_node *node, *out_node;
	u_int nxtid;

	out_node = NULL;
	TAILQ_FOREACH(node, &pci_nodes, next) {
		if (node->data.pci_rc.PciSegmentNumber != seg)
			continue;
		out_node = iort_entry_lookup(node, rid, &nxtid);
		if (out_node != NULL)
			break;
	}

	out_node = iort_smmu_trymap(out_node, outtype, &nxtid);
	if (out_node)
		*outid = nxtid;

	return (out_node);
}

/*
 * Map a named component node to a SMMU node or an ITS node, based on outtype.
 */
static struct iort_node *
iort_named_comp_map(const char *devname, u_int rid, u_int outtype, u_int *outid)
{
	struct iort_node *node, *out_node;
	u_int nxtid;

	out_node = NULL;
	TAILQ_FOREACH(node, &named_nodes, next) {
		if (strstr(node->data.named_comp.DeviceName, devname) == NULL)
			continue;
		out_node = iort_entry_lookup(node, rid, &nxtid);
		if (out_node != NULL)
			break;
	}

	out_node = iort_smmu_trymap(out_node, outtype, &nxtid);
	if (out_node)
		*outid = nxtid;

	return (out_node);
}

#ifdef notyet
/*
 * Not implemented, map a PCIe device to the SMMU it is associated with.
 */
int
acpi_iort_map_smmu(u_int seg, u_int devid, void **smmu, u_int *sid)
{
	/* XXX: convert oref to SMMU device */
	return (ENXIO);
}
#endif

/*
 * Allocate memory for a node, initialize and copy mappings. 'start'
 * argument provides the table start used to calculate the node offset.
 */
static void
iort_copy_data(struct iort_node *node, ACPI_IORT_NODE *node_entry)
{
	ACPI_IORT_ID_MAPPING *map_entry;
	struct iort_map_entry *mapping;
	int i;

	map_entry = ACPI_ADD_PTR(ACPI_IORT_ID_MAPPING, node_entry,
	    node_entry->MappingOffset);
	node->nentries = node_entry->MappingCount;
	node->usecount = 0;
	mapping = malloc(sizeof(*mapping) * node->nentries, M_DEVBUF,
	    M_WAITOK | M_ZERO);
	node->entries.mappings = mapping;
	for (i = 0; i < node->nentries; i++, mapping++, map_entry++) {
		mapping->base = map_entry->InputBase;
		/*
		 * IdCount means "The number of IDs in the range minus one" (ARM DEN 0049D).
		 * We use <= for comparison against this field, so don't add one here.
		 */
		mapping->end = map_entry->InputBase + map_entry->IdCount;
		mapping->outbase = map_entry->OutputBase;
		mapping->out_node_offset = map_entry->OutputReference;
		mapping->flags = map_entry->Flags;
		mapping->out_node = NULL;
	}
}

/*
 * Allocate and copy an ITS group.
 */
static void
iort_copy_its(struct iort_node *node, ACPI_IORT_NODE *node_entry)
{
	struct iort_its_entry *its;
	ACPI_IORT_ITS_GROUP *itsg_entry;
	UINT32 *id;
	int i;

	itsg_entry = (ACPI_IORT_ITS_GROUP *)node_entry->NodeData;
	node->nentries = itsg_entry->ItsCount;
	node->usecount = 0;
	its = malloc(sizeof(*its) * node->nentries, M_DEVBUF, M_WAITOK | M_ZERO);
	node->entries.its = its;
	id = &itsg_entry->Identifiers[0];
	for (i = 0; i < node->nentries; i++, its++, id++) {
		its->its_id = *id;
		its->pxm = -1;
		its->xref = 0;
	}
}

/*
 * Walk the IORT table and add nodes to corresponding list.
 */
static void
iort_add_nodes(ACPI_IORT_NODE *node_entry, u_int node_offset)
{
	ACPI_IORT_ROOT_COMPLEX *pci_rc;
	ACPI_IORT_SMMU *smmu;
	ACPI_IORT_SMMU_V3 *smmu_v3;
	ACPI_IORT_NAMED_COMPONENT *named_comp;
	struct iort_node *node;

	node = malloc(sizeof(*node), M_DEVBUF, M_WAITOK | M_ZERO);
	node->type =  node_entry->Type;
	node->node_offset = node_offset;
	node->revision = node_entry->Revision;

	/* copy nodes depending on type */
	switch(node_entry->Type) {
	case ACPI_IORT_NODE_PCI_ROOT_COMPLEX:
		pci_rc = (ACPI_IORT_ROOT_COMPLEX *)node_entry->NodeData;
		memcpy(&node->data.pci_rc, pci_rc, sizeof(*pci_rc));
		iort_copy_data(node, node_entry);
		TAILQ_INSERT_TAIL(&pci_nodes, node, next);
		break;
	case ACPI_IORT_NODE_SMMU:
		smmu = (ACPI_IORT_SMMU *)node_entry->NodeData;
		memcpy(&node->data.smmu, smmu, sizeof(*smmu));
		iort_copy_data(node, node_entry);
		TAILQ_INSERT_TAIL(&smmu_nodes, node, next);
		break;
	case ACPI_IORT_NODE_SMMU_V3:
		smmu_v3 = (ACPI_IORT_SMMU_V3 *)node_entry->NodeData;
		memcpy(&node->data.smmu_v3, smmu_v3, sizeof(*smmu_v3));
		iort_copy_data(node, node_entry);
		TAILQ_INSERT_TAIL(&smmu_nodes, node, next);
		break;
	case ACPI_IORT_NODE_ITS_GROUP:
		iort_copy_its(node, node_entry);
		TAILQ_INSERT_TAIL(&its_groups, node, next);
		break;
	case ACPI_IORT_NODE_NAMED_COMPONENT:
		named_comp = (ACPI_IORT_NAMED_COMPONENT *)node_entry->NodeData;
		memcpy(&node->data.named_comp, named_comp, sizeof(*named_comp));

		/* Copy name of the node separately. */
		strncpy(node->data.named_comp.DeviceName,
		    named_comp->DeviceName,
		    sizeof(node->data.named_comp.DeviceName));
		node->data.named_comp.DeviceName[31] = 0;

		iort_copy_data(node, node_entry);
		TAILQ_INSERT_TAIL(&named_nodes, node, next);
		break;
	default:
		printf("ACPI: IORT: Dropping unhandled type %u\n",
		    node_entry->Type);
		free(node, M_DEVBUF);
		break;
	}
}

/*
 * For the mapping entry given, walk thru all the possible destination
 * nodes and resolve the output reference.
 */
static void
iort_resolve_node(struct iort_map_entry *entry, int check_smmu)
{
	struct iort_node *node, *np;

	node = NULL;
	if (check_smmu) {
		TAILQ_FOREACH(np, &smmu_nodes, next) {
			if (entry->out_node_offset == np->node_offset) {
				node = np;
				break;
			}
		}
	}
	if (node == NULL) {
		TAILQ_FOREACH(np, &its_groups, next) {
			if (entry->out_node_offset == np->node_offset) {
				node = np;
				break;
			}
		}
	}
	if (node != NULL) {
		node->usecount++;
		entry->out_node = node;
	} else {
		printf("ACPI: IORT: Firmware Bug: no mapping for node %u\n",
		    entry->out_node_offset);
	}
}

/*
 * Resolve all output node references to node pointers.
 */
static void
iort_post_process_mappings(void)
{
	struct iort_node *node;
	int i;

	TAILQ_FOREACH(node, &pci_nodes, next)
		for (i = 0; i < node->nentries; i++)
			iort_resolve_node(&node->entries.mappings[i], TRUE);
	TAILQ_FOREACH(node, &smmu_nodes, next)
		for (i = 0; i < node->nentries; i++)
			iort_resolve_node(&node->entries.mappings[i], FALSE);
	TAILQ_FOREACH(node, &named_nodes, next)
		for (i = 0; i < node->nentries; i++)
			iort_resolve_node(&node->entries.mappings[i], TRUE);
}

/*
 * Walk MADT table, assign PIC xrefs to all ITS entries.
 */
static void
madt_resolve_its_xref(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_MADT_GENERIC_TRANSLATOR *gict;
	struct iort_node *its_node;
	struct iort_its_entry *its_entry;
	u_int xref;
	int i, matches;

        if (entry->Type != ACPI_MADT_TYPE_GENERIC_TRANSLATOR)
		return;

	gict = (ACPI_MADT_GENERIC_TRANSLATOR *)entry;
	matches = 0;
	xref = acpi_its_xref++;
	TAILQ_FOREACH(its_node, &its_groups, next) {
		its_entry = its_node->entries.its;
		for (i = 0; i < its_node->nentries; i++, its_entry++) {
			if (its_entry->its_id == gict->TranslationId) {
				its_entry->xref = xref;
				matches++;
			}
		}
	}
	if (matches == 0)
		printf("ACPI: IORT: Unused ITS block, ID %u\n",
		    gict->TranslationId);
}

/*
 * Walk SRAT, assign proximity to all ITS entries.
 */
static void
srat_resolve_its_pxm(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_SRAT_GIC_ITS_AFFINITY *gicits;
	struct iort_node *its_node;
	struct iort_its_entry *its_entry;
	int *map_counts;
	int i, matches, dom;

	if (entry->Type != ACPI_SRAT_TYPE_GIC_ITS_AFFINITY)
		return;

	matches = 0;
	map_counts = arg;
	gicits = (ACPI_SRAT_GIC_ITS_AFFINITY *)entry;
	dom = acpi_map_pxm_to_vm_domainid(gicits->ProximityDomain);

	/*
	 * Catch firmware and config errors. map_counts keeps a
	 * count of ProximityDomain values mapping to a domain ID
	 */
#if MAXMEMDOM > 1
	if (dom == -1)
		printf("Firmware Error: Proximity Domain %d could not be"
		    " mapped for GIC ITS ID %d!\n",
		    gicits->ProximityDomain, gicits->ItsId);
#endif
	/* use dom + 1 as index to handle the case where dom == -1 */
	i = ++map_counts[dom + 1];
	if (i > 1) {
#ifdef NUMA
		if (dom != -1)
			printf("ERROR: Multiple Proximity Domains map to the"
			    " same NUMA domain %d!\n", dom);
#else
		printf("WARNING: multiple Proximity Domains in SRAT but NUMA"
		    " NOT enabled!\n");
#endif
	}
	TAILQ_FOREACH(its_node, &its_groups, next) {
		its_entry = its_node->entries.its;
		for (i = 0; i < its_node->nentries; i++, its_entry++) {
			if (its_entry->its_id == gicits->ItsId) {
				its_entry->pxm = dom;
				matches++;
			}
		}
	}
	if (matches == 0)
		printf("ACPI: IORT: ITS block %u in SRAT not found in IORT!\n",
		    gicits->ItsId);
}

/*
 * Cross check the ITS Id with MADT and (if available) SRAT.
 */
static int
iort_post_process_its(void)
{
	ACPI_TABLE_MADT *madt;
	ACPI_TABLE_SRAT *srat;
	vm_paddr_t madt_pa, srat_pa;
	int map_counts[MAXMEMDOM + 1] = { 0 };

	/* Check ITS block in MADT */
	madt_pa = acpi_find_table(ACPI_SIG_MADT);
	KASSERT(madt_pa != 0, ("no MADT!"));
	madt = acpi_map_table(madt_pa, ACPI_SIG_MADT);
	KASSERT(madt != NULL, ("can't map MADT!"));
	acpi_walk_subtables(madt + 1, (char *)madt + madt->Header.Length,
	    madt_resolve_its_xref, NULL);
	acpi_unmap_table(madt);

	/* Get proximtiy if available */
	srat_pa = acpi_find_table(ACPI_SIG_SRAT);
	if (srat_pa != 0) {
		srat = acpi_map_table(srat_pa, ACPI_SIG_SRAT);
		KASSERT(srat != NULL, ("can't map SRAT!"));
		acpi_walk_subtables(srat + 1, (char *)srat + srat->Header.Length,
		    srat_resolve_its_pxm, map_counts);
		acpi_unmap_table(srat);
	}
	return (0);
}

/*
 * Find, parse, and save IO Remapping Table ("IORT").
 */
static int
acpi_parse_iort(void *dummy __unused)
{
	ACPI_TABLE_IORT *iort;
	ACPI_IORT_NODE *node_entry;
	vm_paddr_t iort_pa;
	u_int node_offset;

	iort_pa = acpi_find_table(ACPI_SIG_IORT);
	if (iort_pa == 0)
		return (ENXIO);

	iort = acpi_map_table(iort_pa, ACPI_SIG_IORT);
	if (iort == NULL) {
		printf("ACPI: Unable to map the IORT table!\n");
		return (ENXIO);
	}
	for (node_offset = iort->NodeOffset;
	    node_offset < iort->Header.Length;
	    node_offset += node_entry->Length) {
		node_entry = ACPI_ADD_PTR(ACPI_IORT_NODE, iort, node_offset);
		iort_add_nodes(node_entry, node_offset);
	}
	acpi_unmap_table(iort);
	iort_post_process_mappings();
	iort_post_process_its();
	return (0);
}
SYSINIT(acpi_parse_iort, SI_SUB_DRIVERS, SI_ORDER_FIRST, acpi_parse_iort, NULL);

/*
 * Provide ITS ID to PIC xref mapping.
 */
int
acpi_iort_its_lookup(u_int its_id, u_int *xref, int *pxm)
{
	struct iort_node *its_node;
	struct iort_its_entry *its_entry;
	int i;

	TAILQ_FOREACH(its_node, &its_groups, next) {
		its_entry = its_node->entries.its;
		for  (i = 0; i < its_node->nentries; i++, its_entry++) {
			if (its_entry->its_id == its_id) {
				*xref = its_entry->xref;
				*pxm = its_entry->pxm;
				return (0);
			}
		}
	}
	return (ENOENT);
}

/*
 * Find mapping for a PCIe device given segment and device ID
 * returns the XREF for MSI interrupt setup and the device ID to
 * use for the interrupt setup
 */
int
acpi_iort_map_pci_msi(u_int seg, u_int rid, u_int *xref, u_int *devid)
{
	struct iort_node *node;

	node = iort_pci_rc_map(seg, rid, ACPI_IORT_NODE_ITS_GROUP, devid);
	if (node == NULL)
		return (ENOENT);

	/* This should be an ITS node */
	KASSERT(node->type == ACPI_IORT_NODE_ITS_GROUP, ("bad group"));

	/* return first node, we don't handle more than that now. */
	*xref = node->entries.its[0].xref;
	return (0);
}

int
acpi_iort_map_pci_smmuv3(u_int seg, u_int rid, uint64_t *xref, u_int *sid)
{
	ACPI_IORT_SMMU_V3 *smmu;
	struct iort_node *node;

	node = iort_pci_rc_map(seg, rid, ACPI_IORT_NODE_SMMU_V3, sid);
	if (node == NULL)
		return (ENOENT);

	/* This should be an SMMU node. */
	KASSERT(node->type == ACPI_IORT_NODE_SMMU_V3, ("bad node"));

	smmu = (ACPI_IORT_SMMU_V3 *)&node->data.smmu_v3;
	*xref = smmu->BaseAddress;

	return (0);
}

/*
 * Finds mapping for a named node given name and resource ID and returns the
 * XREF for MSI interrupt setup and the device ID to use for the interrupt setup.
 */
int
acpi_iort_map_named_msi(const char *devname, u_int rid, u_int *xref,
    u_int *devid)
{
	struct iort_node *node;

	node = iort_named_comp_map(devname, rid, ACPI_IORT_NODE_ITS_GROUP,
	    devid);
	if (node == NULL)
		return (ENOENT);

	/* This should be an ITS node */
	KASSERT(node->type == ACPI_IORT_NODE_ITS_GROUP, ("bad group"));

	/* Return first node, we don't handle more than that now. */
	*xref = node->entries.its[0].xref;
	return (0);
}

int
acpi_iort_map_named_smmuv3(const char *devname, u_int rid, uint64_t *xref,
    u_int *devid)
{
	ACPI_IORT_SMMU_V3 *smmu;
	struct iort_node *node;

	node = iort_named_comp_map(devname, rid, ACPI_IORT_NODE_SMMU_V3, devid);
	if (node == NULL)
		return (ENOENT);

	/* This should be an SMMU node. */
	KASSERT(node->type == ACPI_IORT_NODE_SMMU_V3, ("bad node"));

	smmu = (ACPI_IORT_SMMU_V3 *)&node->data.smmu_v3;
	*xref = smmu->BaseAddress;

	return (0);
}
