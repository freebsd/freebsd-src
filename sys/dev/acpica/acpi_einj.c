/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025-2026 Netflix, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/actables.h>

#include <dev/acpica/acpiio.h>
#include <dev/acpica/acpivar.h>
#include <dev/acpica/apeivar.h>

#define	ACPI_EINJ_MAX_ACTION	(ACPI_EINJV2_GET_ERROR_TYPE)

struct einj_instruction {
	struct resource_map *res;
	u_int instruction;
	u_int flags;
	u_int size;
	uint64_t value;
	uint64_t mask;
};

struct einj_action {
	struct einj_instruction *instructions;
	u_int num_instructions;
};

struct einj_softc {
	device_t dev;
	ACPI_TABLE_EINJ *einj;
	struct einj_action actions[ACPI_EINJ_MAX_ACTION + 1];

	struct acpi_einj_info info;
	struct resource_map *address_res;
	struct resource_map *vendor_res;

	struct sx lock;
};

static MALLOC_DEFINE(M_EINJ, "einj", "ACPI error injection");

static bool
einj_validate_instruction(u_int i, ACPI_EINJ_ENTRY *e)
{
	ACPI_GENERIC_ADDRESS *gas;
	UINT8 valid_flags;

	valid_flags = 0;
	switch (e->WheaHeader.Instruction) {
	case ACPI_EINJ_WRITE_REGISTER:
	case ACPI_EINJ_WRITE_REGISTER_VALUE:
		valid_flags |= ACPI_EINJ_PRESERVE;
		break;
	case ACPI_EINJ_READ_REGISTER:
	case ACPI_EINJ_READ_REGISTER_VALUE:
		/* PRESERVE_REGISTER is ignored, but not invalid. */
		valid_flags |= ACPI_EINJ_PRESERVE;
		break;
	case ACPI_EINJ_NOOP:
#if 0
		/* Not documented. */
	case ACPI_EINJ_FLUSH_CACHELINE:
#endif
		break;
	default:
		if (bootverbose)
			printf("EINJ: Unknown instruction at index %u\n", i);
		return (false);
	}
	if ((e->WheaHeader.Flags & ~valid_flags) != 0) {
		if (bootverbose)
			printf("EINJ: Invalid instruction flag at index %u\n",
			    i);
		return (false);
	}

	gas = &e->WheaHeader.RegisterRegion;
	switch (gas->SpaceId) {
	case ACPI_ADR_SPACE_SYSTEM_MEMORY:
	case ACPI_ADR_SPACE_SYSTEM_IO:
		break;
	default:
		if (bootverbose)
			printf("EINJ: Unsupported register address space at index %u\n",
			    i);
		return (false);
	}

	/*
	 * The spec seems to suggest sub-bit ranges of registers might
	 * be valid, but punt on handling those until one is actually
	 * encountered in the wild.
	 */
	if (gas->BitOffset != 0) {
		if (bootverbose)
			printf("EINJ: Unsupported register offset at index %u\n",
			    i);
		return (false);
	}
	if (!(gas->BitWidth == 8 || gas->BitWidth == 16 ||
	    gas->BitWidth == 32 || gas->BitWidth == 64)) {
		if (bootverbose)
			printf("EINJ: Unsupported register width at index %u\n",
			    i);
		return (false);
	}
	return (true);
}

static bool
einj_validate_table(ACPI_TABLE_EINJ *einj)
{
	ACPI_EINJ_ENTRY *e;
	u_int i;
	bool seen_set_error_type_with_address;

	if (einj->Header.Revision < 1 || einj->Header.Revision > 2) {
		if (bootverbose)
			printf("EINJ: Unsupported revision %u\n",
			    einj->Header.Revision);
		return (false);
	}
	if (einj->HeaderLength != sizeof(*einj)) {
		if (bootverbose)
			printf("EINJ: Invalid Injection Interface Header Length\n");
		return (false);
	}
	if (einj->Entries == 0) {
		if (bootverbose)
			printf("EINJ: No actions\n");
		return (false);
	}
	if (einj->Header.Length != sizeof(*einj) + einj->Entries * sizeof(*e)) {
		if (bootverbose)
			printf("EINJ: Invalid table length\n");
		return (false);
	}

	e = (ACPI_EINJ_ENTRY *)(einj + 1);
	seen_set_error_type_with_address = false;
	for (i = 0; i < einj->Entries; i++, e++) {
		if (e->WheaHeader.Action > ACPI_EINJ_MAX_ACTION) {
			if (bootverbose)
				printf("EINJ: Invalid action at index %u\n", i);
			return (false);
		}

		if (e->WheaHeader.Action ==
		    ACPI_EINJ_SET_ERROR_TYPE_WITH_ADDRESS) {
			if (seen_set_error_type_with_address) {
				if (bootverbose)
					printf("EINJ: Multiple SET_ERROR_TYPE_WITH_ADDRESS instructions\n");
				return (false);
			}
			seen_set_error_type_with_address = true;
		}

		if (!einj_validate_instruction(i, e))
			return (false);
	}
	return (true);
}

static uint64_t
einj_read_register(struct einj_instruction *inst)
{
	switch (inst->size) {
	case 1:
		return (bus_read_1(inst->res, 0));
	case 2:
		return (bus_read_2(inst->res, 0));
	case 4:
		return (bus_read_4(inst->res, 0));
	case 8:
		return (bus_read_8(inst->res, 0));
	default:
		__assert_unreachable();
	}
}

static void
einj_write_register(struct einj_instruction *inst, uint64_t value)
{
	switch (inst->size) {
	case 1:
		bus_write_1(inst->res, 0, value);
		break;
	case 2:
		bus_write_2(inst->res, 0, value);
		break;
	case 4:
		bus_write_4(inst->res, 0, value);
		break;
	case 8:
		bus_write_8(inst->res, 0, value);
		break;
	default:
		__assert_unreachable();
	}
}

static uint64_t
einj_execute_instruction(struct einj_instruction *inst, uint64_t value)
{
	uint64_t old;

	switch (inst->instruction) {
	case ACPI_EINJ_READ_REGISTER:
	case ACPI_EINJ_READ_REGISTER_VALUE:
		value = einj_read_register(inst);
		value &= inst->mask;
		if (inst->instruction == ACPI_EINJ_READ_REGISTER_VALUE)
			value = (value == inst->value);
		break;
	case ACPI_EINJ_WRITE_REGISTER_VALUE:
		value = inst->value;
		/* FALLTHROUGH */
	case ACPI_EINJ_WRITE_REGISTER:
		value &= inst->mask;
		if (inst->flags & ACPI_EINJ_PRESERVE) {
			old = einj_read_register(inst);
			value |= (old & ~inst->mask);
		}
		einj_write_register(inst, value);
		break;
	case ACPI_EINJ_NOOP:
		break;
	default:
		__assert_unreachable();
	}
	return (value);
}

static uint64_t
einj_execute_instructions(struct einj_instruction *inst, u_int count,
    uint64_t value)
{
	for (u_int i = 0; i < count; i++, inst++) {
		value = einj_execute_instruction(inst, value);
	}
	return (value);
}

static int
einj_execute_action(struct einj_softc *sc, u_int action, uint64_t *value)
{
	struct einj_action *ea;

	if (action >= nitems(sc->actions))
		return (ENOENT);
	ea = &sc->actions[action];
	if (ea->num_instructions == 0)
		return (ENOENT);

	*value = einj_execute_instructions(ea->instructions,
	    ea->num_instructions, *value);
	return (0);
}

static int
einj_trigger_error(struct einj_softc *sc, vm_paddr_t table_pa)
{
	ACPI_EINJ_TRIGGER *table;
	ACPI_EINJ_ENTRY *e;
	size_t table_len;
	uint64_t value;
	int error;

	/* Map the header to obtain the full length. */
	table_len = sizeof(*table);
	table = pmap_mapbios(table_pa, table_len);
	if (table == NULL) {
		device_printf(sc->dev, "failed to map trigger table header\n");
		return (ENXIO);
	}

	if (table->HeaderSize != sizeof(*table)) {
		device_printf(sc->dev, "invalid trigger table header size %u\n",
		    table->HeaderSize);
		error = ENXIO;
		goto out;
	}

	if (table->TableSize <
	    table->HeaderSize + sizeof(*e) * table->EntryCount) {
		device_printf(sc->dev,
		    "trigger table too small (%u) for %u entries\n",
		    table->TableSize, table->EntryCount);
		error = ENXIO;
		goto out;
	}

	if (table->EntryCount == 0) {
		error = 0;
		goto out;
	}

	/* Map the full table. */
	pmap_unmapbios(table, table_len);
	table_len = table->TableSize;
	table = pmap_mapbios(table_pa, table_len);
	if (table == NULL) {
		device_printf(sc->dev, "failed to map trigger table\n");
		return (ENXIO);
	}

	value = 0;
	e = (ACPI_EINJ_ENTRY *)(table + 1);
	for (u_int i = 0; i < table->EntryCount; i++, e++) {
		struct einj_instruction ei;

		if (e->WheaHeader.Action != ACPI_EINJ_TRIGGER_ERROR)
			device_printf(sc->dev,
			    "invalid action %#x for trigger instruction %u\n",
			    e->WheaHeader.Action, i);

		if (e->WheaHeader.Instruction == ACPI_EINJ_NOOP)
			continue;

		ei.res = apei_map_register(sc->dev,
		    &e->WheaHeader.RegisterRegion);
		if (ei.res == NULL) {
			device_printf(sc->dev,
			    "failed to map register for trigger entry %u\n", i);
			error = ENXIO;
			goto out;
		}

		ei.instruction = e->WheaHeader.Instruction;
		ei.flags = e->WheaHeader.Flags;
		ei.value = e->WheaHeader.Value;
		ei.mask = e->WheaHeader.Mask;
		ei.size = e->WheaHeader.RegisterRegion.BitWidth / 8;
		value = einj_execute_instruction(&ei, value);
		apei_unmap_register(sc->dev, ei.res);
	}
	error = 0;
out:
	pmap_unmapbios(table, table_len);
	return (error);
}

static int
einj_set_error(struct einj_softc *sc, const struct acpi_einj_error *err)
{
	uint64_t value;
	int error;

	if ((err->address_flags & ~(ACPI_EINJ_APICID_VALID |
	    ACPI_EINJ_MEMADDRESS_VALID | ACPI_EINJ_PCIE_VALID)) != 0)
		return (EINVAL);
	if (err->address_flags != 0 && sc->address_res == NULL)
		return (EOPNOTSUPP);

	value = 0;
	error = einj_execute_action(sc, ACPI_EINJ_BEGIN_OPERATION, &value);
	if (error != 0) {
		device_printf(sc->dev, "BEGIN_OPERATION failed\n");
		return (error);
	}

	value = err->error_type;
	if (sc->address_res != NULL) {
		error = einj_execute_action(sc,
		    ACPI_EINJ_SET_ERROR_TYPE_WITH_ADDRESS, &value);
		if (error != 0) {
			device_printf(sc->dev,
			    "SET_ERROR_TYPE_WITH_ADDRESS failed\n");
			goto out;
		}

		bus_write_4(sc->address_res,
		    offsetof(ACPI_EINJ_ERROR_TYPE_WITH_ADDR, Flags),
		    err->address_flags);
		bus_write_4(sc->address_res,
		    offsetof(ACPI_EINJ_ERROR_TYPE_WITH_ADDR, ApicId),
		    err->apic_id);
		bus_write_8(sc->address_res,
		    offsetof(ACPI_EINJ_ERROR_TYPE_WITH_ADDR, Address),
		    err->memory_address);
		bus_write_8(sc->address_res,
		    offsetof(ACPI_EINJ_ERROR_TYPE_WITH_ADDR, Range),
		    err->memory_range);
		bus_write_4(sc->address_res,
		    offsetof(ACPI_EINJ_ERROR_TYPE_WITH_ADDR, PcieId),
		    err->pcie_id);
	} else {
		error = einj_execute_action(sc, ACPI_EINJ_SET_ERROR_TYPE,
		    &value);
		if (error != 0) {
			device_printf(sc->dev, "SET_ERROR_TYPE failed\n");
			goto out;
		}
	}

	value = 0;
	error = einj_execute_action(sc, ACPI_EINJ_EXECUTE_OPERATION, &value);
	if (error != 0) {
		device_printf(sc->dev, "EXECUTE_OPERATION failed\n");
		goto out;
	}

	for (;;) {
		value = 0;
		error = einj_execute_action(sc, ACPI_EINJ_CHECK_BUSY_STATUS,
		    &value);
		if (error != 0) {
			device_printf(sc->dev, "CHECK_BUSY_STATUS failed\n");
			goto out;
		}

		if ((value & 1) == 0)
			break;
		DELAY(1000);
	}

	value = 0;
	error = einj_execute_action(sc, ACPI_EINJ_GET_COMMAND_STATUS, &value);
	if (error != 0) {
		device_printf(sc->dev, "GET_COMMAND_STATUS failed\n");
		goto out;
	}

	/* Bits 1:8 of the returned value contain the status code. */
	switch ((value >> 1) & 0xff) {
	case ACPI_EINJ_SUCCESS:
		break;
	case ACPI_EINJ_FAILURE:
		device_printf(sc->dev, "injection of error type %#x failed\n",
		    err->error_type);
		error = ENXIO;
		goto out;
	case ACPI_EINJ_INVALID_ACCESS:
		device_printf(sc->dev,
		    "invalid access while injecting error type %#x\n",
		    err->error_type);
		error = ENXIO;
		goto out;
	default:
		device_printf(sc->dev,
		    "unknown status %ju while injecting error type %#x\n",
		    (uintmax_t)value, err->error_type);
		error = ENXIO;
		goto out;
	}

	value = 0;
	error = einj_execute_action(sc, ACPI_EINJ_GET_TRIGGER_TABLE, &value);
	if (error != 0) {
		device_printf(sc->dev, "GET_TRIGER_TABLE failed\n");
		goto out;
	}

	error = einj_trigger_error(sc, value);
out:
	value = 0;
	(void)einj_execute_action(sc, ACPI_EINJ_END_OPERATION, &value);
	return (error);
}

static int
einj_ioctl(u_long cmd, caddr_t addr, void *arg)
{
	struct einj_softc *sc = arg;
	int error;

	error = 0;
	switch (cmd) {
	case ACPIIO_EINJ_GET_INFO:
		memcpy(addr, &sc->info, sizeof(sc->info));
		break;
	case ACPIIO_EINJ_GET_VENDOR: {
		struct acpi_einj_vendor_info *v = (void *)addr;
		void *buf;

		if (sc->vendor_res == NULL)
			return (ENXIO);
		if (v->len != sc->info.vendor_length)
			return (EINVAL);

		/*
		 * This assumes the region can be safely read via
		 * 4-byte reads.
		 */
		buf = malloc(v->len, M_DEVBUF, M_WAITOK);
		bus_read_region_4(sc->vendor_res, 0, buf, v->len / 4);
		if (v->len % 4 != 0) {
			uint32_t offset;

			offset = rounddown2(v->len, 4);
			bus_read_region_1(sc->vendor_res, offset,
			    (char *)buf + offset, v->len % 4);
		}

		error = copyout(buf, v->buf, v->len);
		free(buf, M_DEVBUF);
		break;
	}
	case ACPIIO_EINJ_SET_ERROR:
		sx_xlock(&sc->lock);
		error = einj_set_error(sc, (struct acpi_einj_error *)addr);
		sx_xunlock(&sc->lock);
		break;
	default:
		error = ENOIOCTL;
		break;
	}
	return (error);
}

static void
einj_identify(driver_t *driver, device_t parent)
{
	ACPI_TABLE_HEADER *einj;
	ACPI_STATUS status;
	bool valid;

	if (device_find_child(parent, "einj", DEVICE_UNIT_ANY) != NULL)
		return;

	status = AcpiGetTable(ACPI_SIG_EINJ, 0, &einj);
	if (ACPI_FAILURE(status))
		return;
	valid = einj_validate_table((ACPI_TABLE_EINJ *)einj);
	AcpiPutTable(einj);
	if (!valid)
		return;

	BUS_ADD_CHILD(parent, 1, "einj", DEVICE_UNIT_ANY);
}

static bool
einj_parse_table(struct einj_softc *sc, ACPI_GENERIC_ADDRESS *address_gas)
{
	ACPI_EINJ_ENTRY *e;
	struct resource_map *res;
	u_int i;

	e = (ACPI_EINJ_ENTRY *)(sc->einj + 1);
	for (i = 0; i < sc->einj->Entries; i++, e++) {
		struct einj_action *ea = &sc->actions[e->WheaHeader.Action];
		struct einj_instruction *ei;

		switch (e->WheaHeader.Instruction) {
		case ACPI_EINJ_NOOP:
			res = NULL;
			break;
		default:
			res = apei_map_register(sc->dev,
			    &e->WheaHeader.RegisterRegion);
			if (res == NULL)
				return (false);
		}

		if (e->WheaHeader.Action ==
		    ACPI_EINJ_SET_ERROR_TYPE_WITH_ADDRESS)
			*address_gas = e->WheaHeader.RegisterRegion;

		ea->instructions = realloc(ea->instructions,
		    sizeof(*ea->instructions) * (ea->num_instructions + 1),
		    M_EINJ, M_WAITOK);
		ei = &ea->instructions[ea->num_instructions];
		ei->res = res;
		ei->instruction = e->WheaHeader.Instruction;
		ei->flags = e->WheaHeader.Flags;
		ei->value = e->WheaHeader.Value;
		ei->mask = e->WheaHeader.Mask;
		ei->size = e->WheaHeader.RegisterRegion.BitWidth / 8;
		ea->num_instructions++;
	}
	return (true);
}

/*
 * SET_ERROR_TYPE_WITH_ADDRESS points to a data structure, but the
 * original action only points to the first word.  Expand the mapping
 * to cover the entire structure and initialize the cached copy of the
 * structure.
 */
static int
einj_parse_set_error_with_address(struct einj_softc *sc,
    const ACPI_GENERIC_ADDRESS *gas)
{
	struct resource_map *res;
	struct einj_action *ea;
	uint32_t vendor_offset;

	ea = &sc->actions[ACPI_EINJ_SET_ERROR_TYPE_WITH_ADDRESS];
	if (ea->num_instructions == 0)
		return (0);

	/* Map the SET_ERROR_TYPE_WITH_ADDRESS structure. */
	res = apei_map_memory(sc->dev, gas->Address,
	    sizeof(ACPI_EINJ_ERROR_TYPE_WITH_ADDR));
	if (res == NULL) {
		device_printf(sc->dev,
		    "failed to map SET_ERROR_TYPE_WITH_ADDRESS structure\n");
		return (ENXIO);
	}
	sc->address_res = res;

	/* Map the Vendor extension structure if it exists. */
	if ((sc->info.error_type & ACPI_EINJ_VENDOR_DEFINED) == 0)
		return (0);

	vendor_offset = bus_read_4(res,
	    offsetof(ACPI_EINJ_ERROR_TYPE_WITH_ADDR, VendorStructOffset));

	if (vendor_offset < sizeof(ACPI_EINJ_ERROR_TYPE_WITH_ADDR)) {
		device_printf(sc->dev,
		    "Vendor Error extension overlaps base structure\n");
		return (0);
	}

	res = apei_map_memory(sc->dev, gas->Address + vendor_offset,
	    sizeof(ACPI_EINJ_VENDOR));
	if (res == NULL) {
		device_printf(sc->dev,
		    "failed to map Vendor Error extension structure\n");
		return (0);
	}

	sc->info.vendor_length =
	    bus_read_4(res, offsetof(ACPI_EINJ_VENDOR, Length));

	if (sc->info.vendor_length < sizeof(ACPI_EINJ_VENDOR)) {
		if (bootverbose || sc->info.vendor_length != 0)
			device_printf(sc->dev,
			    "invalid Vendor Error extension structure length\n");
		apei_unmap_register(sc->dev, res);
		return (0);
	}

	if (sc->info.vendor_length > sizeof(ACPI_EINJ_VENDOR)) {
		apei_unmap_register(sc->dev, res);
		res = apei_map_memory(sc->dev, gas->Address + vendor_offset,
		    sc->info.vendor_length);
		if (res == NULL) {
			device_printf(sc->dev,
			    "failed to map Vendor Error extension structure\n");
			return (0);
		}
	}
	sc->vendor_res = res;

	return (0);
}

static void
einj_cleanup(struct einj_softc *sc)
{
	struct einj_action *ea;

	/* This is idempotent if the handler isn't registered. */
	acpi_deregister_ioctls(einj_ioctl);

	ea = sc->actions;
	for (u_int i = 0; i < nitems(sc->actions); i++, ea++) {
		struct einj_instruction *ei;

		ei = ea->instructions;
		for (u_int j = 0; j < ea->num_instructions; j++, ei++) {
			if (ei->res != NULL)
				apei_unmap_register(sc->dev, ei->res);
		}
		free(ea->instructions, M_EINJ);
	}

	if (sc->address_res)
		apei_unmap_register(sc->dev, sc->address_res);
	if (sc->vendor_res)
		apei_unmap_register(sc->dev, sc->vendor_res);
	AcpiPutTable((ACPI_TABLE_HEADER *)sc->einj);
	sx_destroy(&sc->lock);
}

static int
einj_probe(device_t dev)
{
	device_set_desc(dev, "ACPI Error Injection Interface");
	return (BUS_PROBE_GENERIC);
}

static int
einj_attach(device_t dev)
{
	struct einj_softc *sc = device_get_softc(dev);
	ACPI_TABLE_HEADER *hdr;
	ACPI_GENERIC_ADDRESS address_gas;
	ACPI_STATUS status;
	uint64_t value;
	int error;

	sc->dev = dev;
	status = AcpiGetTable(ACPI_SIG_EINJ, 0, &hdr);
	if (ACPI_FAILURE(status)) {
		device_printf(dev, "Failed to read " ACPI_SIG_EINJ " table\n");
		return (ENXIO);
	}
	sx_init(&sc->lock, "einj");
	sc->einj = (ACPI_TABLE_EINJ *)hdr;

	if (!einj_validate_table(sc->einj)) {
		device_printf(dev, "Invalid " ACPI_SIG_EINJ " table\n");
		goto out;
	}
	if (!einj_parse_table(sc, &address_gas))
		goto out;

	value = 0;
	error = einj_execute_action(sc, ACPI_EINJ_GET_ERROR_TYPE, &value);
	if (error != 0) {
		device_printf(dev, "Failed to fetch supported error types\n");
		goto out;
	}
	sc->info.error_type = value;

	error = einj_parse_set_error_with_address(sc, &address_gas);
	if (error != 0)
		goto out;

	error = acpi_register_ioctl(ACPIIO_EINJ_GET_INFO, einj_ioctl, sc);
	if (error == 0)
		error = acpi_register_ioctl(ACPIIO_EINJ_GET_VENDOR, einj_ioctl,
		    sc);
	if (error == 0)
		error = acpi_register_ioctl(ACPIIO_EINJ_SET_ERROR, einj_ioctl,
		    sc);
	if (error != 0) {
		device_printf(dev, "Failed to register ioctl handler\n");
		goto out;
	}
	return (0);
out:
	einj_cleanup(sc);
	return (ENXIO);
}

static int
einj_detach(device_t dev)
{
	struct einj_softc *sc = device_get_softc(dev);

	einj_cleanup(sc);
	return (0);
}

static device_method_t einj_methods[] = {
	DEVMETHOD(device_identify, einj_identify),
	DEVMETHOD(device_probe, einj_probe),
	DEVMETHOD(device_attach, einj_attach),
	DEVMETHOD(device_detach, einj_detach),
	DEVMETHOD_END
};

static driver_t einj_driver = {
	"einj",
	einj_methods,
	sizeof(struct einj_softc)
};

DRIVER_MODULE(einj, apei, einj_driver, NULL, NULL);
MODULE_DEPEND(einj, acpi, 1, 1, 1);
