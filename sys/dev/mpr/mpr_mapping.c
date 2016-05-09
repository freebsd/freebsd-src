/*-
 * Copyright (c) 2011-2015 LSI Corp.
 * Copyright (c) 2013-2016 Avago Technologies
 * All rights reserved.
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
 *
 * Avago Technologies (LSI) MPT-Fusion Host Adapter FreeBSD
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* TODO Move headers to mprvar */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/sysctl.h>
#include <sys/eventhandler.h>
#include <sys/uio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/mpr/mpi/mpi2_type.h>
#include <dev/mpr/mpi/mpi2.h>
#include <dev/mpr/mpi/mpi2_ioc.h>
#include <dev/mpr/mpi/mpi2_sas.h>
#include <dev/mpr/mpi/mpi2_cnfg.h>
#include <dev/mpr/mpi/mpi2_init.h>
#include <dev/mpr/mpi/mpi2_tool.h>
#include <dev/mpr/mpr_ioctl.h>
#include <dev/mpr/mprvar.h>
#include <dev/mpr/mpr_mapping.h>

/**
 * _mapping_clear_entry - Clear a particular mapping entry.
 * @map_entry: map table entry
 *
 * Returns nothing.
 */
static inline void
_mapping_clear_map_entry(struct dev_mapping_table *map_entry)
{
	map_entry->physical_id = 0;
	map_entry->device_info = 0;
	map_entry->phy_bits = 0;
	map_entry->dpm_entry_num = MPR_DPM_BAD_IDX;
	map_entry->dev_handle = 0;
	map_entry->channel = -1;
	map_entry->id = -1;
	map_entry->missing_count = 0;
	map_entry->init_complete = 0;
	map_entry->TLR_bits = (u8)MPI2_SCSIIO_CONTROL_NO_TLR;
}

/**
 * _mapping_clear_enc_entry - Clear a particular enclosure table entry.
 * @enc_entry: enclosure table entry
 *
 * Returns nothing.
 */
static inline void
_mapping_clear_enc_entry(struct enc_mapping_table *enc_entry)
{
	enc_entry->enclosure_id = 0;
	enc_entry->start_index = MPR_MAPTABLE_BAD_IDX;
	enc_entry->phy_bits = 0;
	enc_entry->dpm_entry_num = MPR_DPM_BAD_IDX;
	enc_entry->enc_handle = 0;
	enc_entry->num_slots = 0;
	enc_entry->start_slot = 0;
	enc_entry->missing_count = 0;
	enc_entry->removal_flag = 0;
	enc_entry->skip_search = 0;
	enc_entry->init_complete = 0;
}

/**
 * _mapping_commit_enc_entry - write a particular enc entry in DPM page0.
 * @sc: per adapter object
 * @enc_entry: enclosure table entry
 *
 * Returns 0 for success, non-zero for failure.
 */
static int
_mapping_commit_enc_entry(struct mpr_softc *sc,
    struct enc_mapping_table *et_entry)
{
	Mpi2DriverMap0Entry_t *dpm_entry;
	struct dev_mapping_table *mt_entry;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2DriverMappingPage0_t config_page;

	if (!sc->is_dpm_enable)
		return 0;

	memset(&config_page, 0, sizeof(Mpi2DriverMappingPage0_t));
	memcpy(&config_page.Header, (u8 *) sc->dpm_pg0,
	    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
	dpm_entry = (Mpi2DriverMap0Entry_t *)((u8 *)sc->dpm_pg0 +
	    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
	dpm_entry += et_entry->dpm_entry_num;
	dpm_entry->PhysicalIdentifier.Low =
	    ( 0xFFFFFFFF & et_entry->enclosure_id);
	dpm_entry->PhysicalIdentifier.High =
	    ( et_entry->enclosure_id >> 32);
	mt_entry = &sc->mapping_table[et_entry->start_index];
	dpm_entry->DeviceIndex = htole16(mt_entry->id);
	dpm_entry->MappingInformation = et_entry->num_slots;
	dpm_entry->MappingInformation <<= MPI2_DRVMAP0_MAPINFO_SLOT_SHIFT;
	dpm_entry->MappingInformation |= et_entry->missing_count;
	dpm_entry->MappingInformation = htole16(dpm_entry->MappingInformation);
	dpm_entry->PhysicalBitsMapping = htole32(et_entry->phy_bits);
	dpm_entry->Reserved1 = 0;

	memcpy(&config_page.Entry, (u8 *)dpm_entry,
	    sizeof(Mpi2DriverMap0Entry_t));
	if (mpr_config_set_dpm_pg0(sc, &mpi_reply, &config_page,
	    et_entry->dpm_entry_num)) {
		printf("%s: write of dpm entry %d for enclosure failed\n",
		    __func__, et_entry->dpm_entry_num);
		dpm_entry->MappingInformation = le16toh(dpm_entry->
		    MappingInformation);
		dpm_entry->DeviceIndex = le16toh(dpm_entry->DeviceIndex);
		dpm_entry->PhysicalBitsMapping =
		    le32toh(dpm_entry->PhysicalBitsMapping);
		return -1;
	}
	dpm_entry->MappingInformation = le16toh(dpm_entry->
	    MappingInformation);
	dpm_entry->DeviceIndex = le16toh(dpm_entry->DeviceIndex);
	dpm_entry->PhysicalBitsMapping =
	    le32toh(dpm_entry->PhysicalBitsMapping);
	return 0;
}

/**
 * _mapping_commit_map_entry - write a particular map table entry in DPM page0.
 * @sc: per adapter object
 * @enc_entry: enclosure table entry
 *
 * Returns 0 for success, non-zero for failure.
 */

static int
_mapping_commit_map_entry(struct mpr_softc *sc,
    struct dev_mapping_table *mt_entry)
{
	Mpi2DriverMap0Entry_t *dpm_entry;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2DriverMappingPage0_t config_page;

	if (!sc->is_dpm_enable)
		return 0;

	memset(&config_page, 0, sizeof(Mpi2DriverMappingPage0_t));
	memcpy(&config_page.Header, (u8 *)sc->dpm_pg0,
	    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
	dpm_entry = (Mpi2DriverMap0Entry_t *)((u8 *) sc->dpm_pg0 +
	    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
	dpm_entry = dpm_entry + mt_entry->dpm_entry_num;
	dpm_entry->PhysicalIdentifier.Low = (0xFFFFFFFF &
	    mt_entry->physical_id);
	dpm_entry->PhysicalIdentifier.High = (mt_entry->physical_id >> 32);
	dpm_entry->DeviceIndex = htole16(mt_entry->id);
	dpm_entry->MappingInformation = htole16(mt_entry->missing_count);
	dpm_entry->PhysicalBitsMapping = 0;
	dpm_entry->Reserved1 = 0;
	dpm_entry->MappingInformation = htole16(dpm_entry->MappingInformation);
	memcpy(&config_page.Entry, (u8 *)dpm_entry,
	    sizeof(Mpi2DriverMap0Entry_t));
	if (mpr_config_set_dpm_pg0(sc, &mpi_reply, &config_page,
	    mt_entry->dpm_entry_num)) {
		printf("%s: write of dpm entry %d for device failed\n",
		    __func__, mt_entry->dpm_entry_num);
		dpm_entry->MappingInformation = le16toh(dpm_entry->
		    MappingInformation);
		dpm_entry->DeviceIndex = le16toh(dpm_entry->DeviceIndex);
		return -1;
	}

	dpm_entry->MappingInformation = le16toh(dpm_entry->MappingInformation);
	dpm_entry->DeviceIndex = le16toh(dpm_entry->DeviceIndex);
	return 0;
}

/**
 * _mapping_get_ir_maprange - get start and end index for IR map range.
 * @sc: per adapter object
 * @start_idx: place holder for start index
 * @end_idx: place holder for end index
 *
 * The IR volumes can be mapped either at start or end of the mapping table
 * this function gets the detail of where IR volume mapping starts and ends
 * in the device mapping table
 *
 * Returns nothing.
 */
static void
_mapping_get_ir_maprange(struct mpr_softc *sc, u32 *start_idx, u32 *end_idx)
{
	u16 volume_mapping_flags;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);

	volume_mapping_flags = le16toh(sc->ioc_pg8.IRVolumeMappingFlags) &
	    MPI2_IOCPAGE8_IRFLAGS_MASK_VOLUME_MAPPING_MODE;
	if (volume_mapping_flags == MPI2_IOCPAGE8_IRFLAGS_LOW_VOLUME_MAPPING) {
		*start_idx = 0;
		if (ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_RESERVED_TARGETID_0)
			*start_idx = 1;
	} else
		*start_idx = sc->max_devices - sc->max_volumes;
	*end_idx = *start_idx + sc->max_volumes - 1;
}

/**
 * _mapping_get_enc_idx_from_id - get enclosure index from enclosure ID
 * @sc: per adapter object
 * @enc_id: enclosure logical identifier
 *
 * Returns the index of enclosure entry on success or bad index.
 */
static u8
_mapping_get_enc_idx_from_id(struct mpr_softc *sc, u64 enc_id,
    u64 phy_bits)
{
	struct enc_mapping_table *et_entry;
	u8 enc_idx = 0;

	for (enc_idx = 0; enc_idx < sc->num_enc_table_entries; enc_idx++) {
		et_entry = &sc->enclosure_table[enc_idx];
		if ((et_entry->enclosure_id == le64toh(enc_id)) &&
		    (!et_entry->phy_bits || (et_entry->phy_bits &
		    le32toh(phy_bits))))
			return enc_idx;
	}
	return MPR_ENCTABLE_BAD_IDX;
}

/**
 * _mapping_get_enc_idx_from_handle - get enclosure index from handle
 * @sc: per adapter object
 * @enc_id: enclosure handle
 *
 * Returns the index of enclosure entry on success or bad index.
 */
static u8
_mapping_get_enc_idx_from_handle(struct mpr_softc *sc, u16 handle)
{
	struct enc_mapping_table *et_entry;
	u8 enc_idx = 0;

	for (enc_idx = 0; enc_idx < sc->num_enc_table_entries; enc_idx++) {
		et_entry = &sc->enclosure_table[enc_idx];
		if (et_entry->missing_count)
			continue;
		if (et_entry->enc_handle == handle)
			return enc_idx;
	}
	return MPR_ENCTABLE_BAD_IDX;
}

/**
 * _mapping_get_high_missing_et_idx - get missing enclosure index
 * @sc: per adapter object
 *
 * Search through the enclosure table and identifies the enclosure entry
 * with high missing count and returns it's index
 *
 * Returns the index of enclosure entry on success or bad index.
 */
static u8
_mapping_get_high_missing_et_idx(struct mpr_softc *sc)
{
	struct enc_mapping_table *et_entry;
	u8 high_missing_count = 0;
	u8 enc_idx, high_idx = MPR_ENCTABLE_BAD_IDX;

	for (enc_idx = 0; enc_idx < sc->num_enc_table_entries; enc_idx++) {
		et_entry = &sc->enclosure_table[enc_idx];
		if ((et_entry->missing_count > high_missing_count) &&
		    !et_entry->skip_search) {
			high_missing_count =  et_entry->missing_count;
			high_idx = enc_idx;
		}
	}
	return high_idx;
}

/**
 * _mapping_get_high_missing_mt_idx - get missing map table index
 * @sc: per adapter object
 *
 * Search through the map table and identifies the device entry
 * with high missing count and returns it's index
 *
 * Returns the index of map table entry on success or bad index.
 */
static u32
_mapping_get_high_missing_mt_idx(struct mpr_softc *sc)
{
	u32 map_idx, high_idx = MPR_ENCTABLE_BAD_IDX;
	u8 high_missing_count = 0;
	u32 start_idx, end_idx, start_idx_ir, end_idx_ir;
	struct dev_mapping_table *mt_entry;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);

	start_idx = 0;
	start_idx_ir = 0;
	end_idx_ir = 0;
	end_idx = sc->max_devices;
	if (ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_RESERVED_TARGETID_0)
		start_idx = 1;
	if (sc->ir_firmware) {
		_mapping_get_ir_maprange(sc, &start_idx_ir, &end_idx_ir);
		if (start_idx == start_idx_ir)
			start_idx = end_idx_ir + 1;
		else
			end_idx = start_idx_ir;
	}
	mt_entry = &sc->mapping_table[start_idx];
	for (map_idx = start_idx; map_idx < end_idx; map_idx++, mt_entry++) {
		if (mt_entry->missing_count > high_missing_count) {
			high_missing_count =  mt_entry->missing_count;
			high_idx = map_idx;
		}
	}
	return high_idx;
}

/**
 * _mapping_get_ir_mt_idx_from_wwid - get map table index from volume WWID
 * @sc: per adapter object
 * @wwid: world wide unique ID of the volume
 *
 * Returns the index of map table entry on success or bad index.
 */
static u32
_mapping_get_ir_mt_idx_from_wwid(struct mpr_softc *sc, u64 wwid)
{
	u32 start_idx, end_idx, map_idx;
	struct dev_mapping_table *mt_entry;

	_mapping_get_ir_maprange(sc, &start_idx, &end_idx);
	mt_entry = &sc->mapping_table[start_idx];
	for (map_idx  = start_idx; map_idx <= end_idx; map_idx++, mt_entry++)
		if (mt_entry->physical_id == wwid)
			return map_idx;

	return MPR_MAPTABLE_BAD_IDX;
}

/**
 * _mapping_get_mt_idx_from_id - get map table index from a device ID
 * @sc: per adapter object
 * @dev_id: device identifer (SAS Address)
 *
 * Returns the index of map table entry on success or bad index.
 */
static u32
_mapping_get_mt_idx_from_id(struct mpr_softc *sc, u64 dev_id)
{
	u32 map_idx;
	struct dev_mapping_table *mt_entry;

	for (map_idx = 0; map_idx < sc->max_devices; map_idx++) {
		mt_entry = &sc->mapping_table[map_idx];
		if (mt_entry->physical_id == dev_id)
			return map_idx;
	}
	return MPR_MAPTABLE_BAD_IDX;
}

/**
 * _mapping_get_ir_mt_idx_from_handle - get map table index from volume handle
 * @sc: per adapter object
 * @wwid: volume device handle
 *
 * Returns the index of map table entry on success or bad index.
 */
static u32
_mapping_get_ir_mt_idx_from_handle(struct mpr_softc *sc, u16 volHandle)
{
	u32 start_idx, end_idx, map_idx;
	struct dev_mapping_table *mt_entry;

	_mapping_get_ir_maprange(sc, &start_idx, &end_idx);
	mt_entry = &sc->mapping_table[start_idx];
	for (map_idx = start_idx; map_idx <= end_idx; map_idx++, mt_entry++)
		if (mt_entry->dev_handle == volHandle)
			return map_idx;

	return MPR_MAPTABLE_BAD_IDX;
}

/**
 * _mapping_get_mt_idx_from_handle - get map table index from handle
 * @sc: per adapter object
 * @dev_id: device handle
 *
 * Returns the index of map table entry on success or bad index.
 */
static u32
_mapping_get_mt_idx_from_handle(struct mpr_softc *sc, u16 handle)
{
	u32 map_idx;
	struct dev_mapping_table *mt_entry;

	for (map_idx = 0; map_idx < sc->max_devices; map_idx++) {
		mt_entry = &sc->mapping_table[map_idx];
		if (mt_entry->dev_handle == handle)
			return map_idx;
	}
	return MPR_MAPTABLE_BAD_IDX;
}

/**
 * _mapping_get_free_ir_mt_idx - get first free index for a volume
 * @sc: per adapter object
 *
 * Search through mapping table for free index for a volume and if no free
 * index then looks for a volume with high mapping index
 *
 * Returns the index of map table entry on success or bad index.
 */
static u32
_mapping_get_free_ir_mt_idx(struct mpr_softc *sc)
{
	u8 high_missing_count = 0;
	u32 start_idx, end_idx, map_idx;
	u32 high_idx = MPR_MAPTABLE_BAD_IDX;
	struct dev_mapping_table *mt_entry;

	_mapping_get_ir_maprange(sc, &start_idx, &end_idx);

	mt_entry = &sc->mapping_table[start_idx];
	for (map_idx  = start_idx; map_idx <= end_idx; map_idx++, mt_entry++)
		if (!(mt_entry->device_info & MPR_MAP_IN_USE))
			return map_idx;

	mt_entry = &sc->mapping_table[start_idx];
	for (map_idx  = start_idx; map_idx <= end_idx; map_idx++, mt_entry++) {
		if (mt_entry->missing_count > high_missing_count) {
			high_missing_count = mt_entry->missing_count;
			high_idx = map_idx;
		}
	}
	return high_idx;
}

/**
 * _mapping_get_free_mt_idx - get first free index for a device
 * @sc: per adapter object
 * @start_idx: offset in the table to start search
 *
 * Returns the index of map table entry on success or bad index.
 */
static u32
_mapping_get_free_mt_idx(struct mpr_softc *sc, u32 start_idx)
{
	u32 map_idx, max_idx = sc->max_devices;
	struct dev_mapping_table *mt_entry = &sc->mapping_table[start_idx];
	u16 volume_mapping_flags;

	volume_mapping_flags = le16toh(sc->ioc_pg8.IRVolumeMappingFlags) &
	    MPI2_IOCPAGE8_IRFLAGS_MASK_VOLUME_MAPPING_MODE;
	if (sc->ir_firmware && (volume_mapping_flags ==
	    MPI2_IOCPAGE8_IRFLAGS_HIGH_VOLUME_MAPPING))
		max_idx -= sc->max_volumes;
	for (map_idx  = start_idx; map_idx < max_idx; map_idx++, mt_entry++)
		if (!(mt_entry->device_info & (MPR_MAP_IN_USE |
		    MPR_DEV_RESERVED)))
			return map_idx;

	return MPR_MAPTABLE_BAD_IDX;
}

/**
 * _mapping_get_dpm_idx_from_id - get DPM index from ID
 * @sc: per adapter object
 * @id: volume WWID or enclosure ID or device ID
 *
 * Returns the index of DPM entry on success or bad index.
 */
static u16
_mapping_get_dpm_idx_from_id(struct mpr_softc *sc, u64 id, u32 phy_bits)
{
	u16 entry_num;
	uint64_t PhysicalIdentifier;
	Mpi2DriverMap0Entry_t *dpm_entry;

	dpm_entry = (Mpi2DriverMap0Entry_t *)((u8 *)sc->dpm_pg0 +
	    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
	PhysicalIdentifier = dpm_entry->PhysicalIdentifier.High;
	PhysicalIdentifier = (PhysicalIdentifier << 32) | 
	    dpm_entry->PhysicalIdentifier.Low;
	for (entry_num = 0; entry_num < sc->max_dpm_entries; entry_num++,
	    dpm_entry++)
		if ((id == PhysicalIdentifier) &&
		    (!phy_bits || !dpm_entry->PhysicalBitsMapping ||
		    (phy_bits & dpm_entry->PhysicalBitsMapping)))
			return entry_num;

	return MPR_DPM_BAD_IDX;
}


/**
 * _mapping_get_free_dpm_idx - get first available DPM index
 * @sc: per adapter object
 *
 * Returns the index of DPM entry on success or bad index.
 */
static u32
_mapping_get_free_dpm_idx(struct mpr_softc *sc)
{
	u16 entry_num;

	for (entry_num = 0; entry_num < sc->max_dpm_entries; entry_num++) {
		if (!sc->dpm_entry_used[entry_num])
			return entry_num;
	}
	return MPR_DPM_BAD_IDX;
}

/**
 * _mapping_update_ir_missing_cnt - Updates missing count for a volume
 * @sc: per adapter object
 * @map_idx: map table index of the volume
 * @element: IR configuration change element
 * @wwid: IR volume ID.
 *
 * Updates the missing count in the map table and in the DPM entry for a volume
 *
 * Returns nothing.
 */
static void
_mapping_update_ir_missing_cnt(struct mpr_softc *sc, u32 map_idx,
    Mpi2EventIrConfigElement_t *element, u64 wwid)
{
	struct dev_mapping_table *mt_entry;
	u8 missing_cnt, reason = element->ReasonCode;
	u16 dpm_idx;
	Mpi2DriverMap0Entry_t *dpm_entry;

	if (!sc->is_dpm_enable)
		return;
	mt_entry = &sc->mapping_table[map_idx];
	if (reason == MPI2_EVENT_IR_CHANGE_RC_ADDED) {
		mt_entry->missing_count = 0;
	} else if (reason == MPI2_EVENT_IR_CHANGE_RC_VOLUME_CREATED) {
		mt_entry->missing_count = 0;
		mt_entry->init_complete = 0;
	} else if ((reason == MPI2_EVENT_IR_CHANGE_RC_REMOVED) ||
	    (reason == MPI2_EVENT_IR_CHANGE_RC_VOLUME_DELETED)) {
		if (!mt_entry->init_complete) {
			if (mt_entry->missing_count < MPR_MAX_MISSING_COUNT)
				mt_entry->missing_count++;
			else
				mt_entry->init_complete = 1;
		}
		if (!mt_entry->missing_count)
			mt_entry->missing_count++;
		mt_entry->dev_handle = 0;
	}

	dpm_idx = mt_entry->dpm_entry_num;
	if (dpm_idx == MPR_DPM_BAD_IDX) {
		if ((reason == MPI2_EVENT_IR_CHANGE_RC_ADDED) ||
		    (reason == MPI2_EVENT_IR_CHANGE_RC_REMOVED))
			dpm_idx = _mapping_get_dpm_idx_from_id(sc,
			    mt_entry->physical_id, 0);
		else if (reason == MPI2_EVENT_IR_CHANGE_RC_VOLUME_DELETED)
			return;
	}
	if (dpm_idx != MPR_DPM_BAD_IDX) {
		dpm_entry = (Mpi2DriverMap0Entry_t *)((u8 *)sc->dpm_pg0 +
		    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
		dpm_entry += dpm_idx;
		missing_cnt = dpm_entry->MappingInformation &
		    MPI2_DRVMAP0_MAPINFO_MISSING_MASK;
		if ((mt_entry->physical_id ==
		    le64toh((u64)dpm_entry->PhysicalIdentifier.High |
		    dpm_entry->PhysicalIdentifier.Low)) && (missing_cnt ==
		    mt_entry->missing_count))
			mt_entry->init_complete = 1;
	} else {
		dpm_idx = _mapping_get_free_dpm_idx(sc);
		mt_entry->init_complete = 0;
	}

	if ((dpm_idx != MPR_DPM_BAD_IDX) && !mt_entry->init_complete) {
		mt_entry->init_complete = 1;
		mt_entry->dpm_entry_num = dpm_idx;
		dpm_entry = (Mpi2DriverMap0Entry_t *)((u8 *)sc->dpm_pg0 +
		    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
		dpm_entry += dpm_idx;
		dpm_entry->PhysicalIdentifier.Low =
		    (0xFFFFFFFF & mt_entry->physical_id);
		dpm_entry->PhysicalIdentifier.High =
		    (mt_entry->physical_id >> 32);
		dpm_entry->DeviceIndex = map_idx;
		dpm_entry->MappingInformation = mt_entry->missing_count;
		dpm_entry->PhysicalBitsMapping = 0;
		dpm_entry->Reserved1 = 0;
		sc->dpm_flush_entry[dpm_idx] = 1;
		sc->dpm_entry_used[dpm_idx] = 1;
	} else if (dpm_idx == MPR_DPM_BAD_IDX) {
		printf("%s: no space to add entry in DPM table\n", __func__);
		mt_entry->init_complete = 1;
	}
}

/**
 * _mapping_add_to_removal_table - mark an entry for removal
 * @sc: per adapter object
 * @handle: Handle of enclosures/device/volume
 *
 * Adds the handle or DPM entry number in removal table.
 *
 * Returns nothing.
 */
static void
_mapping_add_to_removal_table(struct mpr_softc *sc, u16 handle,
    u16 dpm_idx)
{
	struct map_removal_table *remove_entry;
	u32 i;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);

	remove_entry = sc->removal_table;

	for (i = 0; i < sc->max_devices; i++, remove_entry++) {
		if (remove_entry->dev_handle || remove_entry->dpm_entry_num !=
		    MPR_DPM_BAD_IDX)
			continue;
		if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
		    MPI2_IOCPAGE8_FLAGS_ENCLOSURE_SLOT_MAPPING) {
			if (dpm_idx)
				remove_entry->dpm_entry_num = dpm_idx;
			if (remove_entry->dpm_entry_num == MPR_DPM_BAD_IDX)
				remove_entry->dev_handle = handle;
		} else if ((ioc_pg8_flags &
		    MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
		    MPI2_IOCPAGE8_FLAGS_DEVICE_PERSISTENCE_MAPPING)
			remove_entry->dev_handle = handle;
		break;
	}

}

/**
 * _mapping_update_missing_count - Update missing count for a device
 * @sc: per adapter object
 * @topo_change: Topology change event entry
 *
 * Search through the topology change list and if any device is found not
 * responding it's associated map table entry and DPM entry is updated
 *
 * Returns nothing.
 */
static void
_mapping_update_missing_count(struct mpr_softc *sc,
    struct _map_topology_change *topo_change)
{
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);
	u8 entry;
	struct _map_phy_change *phy_change;
	u32 map_idx;
	struct dev_mapping_table *mt_entry;
	Mpi2DriverMap0Entry_t *dpm_entry;

	for (entry = 0; entry < topo_change->num_entries; entry++) {
		phy_change = &topo_change->phy_details[entry];
		if (!phy_change->dev_handle || (phy_change->reason !=
		    MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING))
			continue;
		map_idx = _mapping_get_mt_idx_from_handle(sc, phy_change->
		    dev_handle);
		phy_change->is_processed = 1;
		if (map_idx == MPR_MAPTABLE_BAD_IDX) {
			printf("%s: device is already removed from mapping "
			    "table\n", __func__);
			continue;
		}
		mt_entry = &sc->mapping_table[map_idx];
		if (!mt_entry->init_complete) {
			if (mt_entry->missing_count < MPR_MAX_MISSING_COUNT)
				mt_entry->missing_count++;
			else
				mt_entry->init_complete = 1;
		}
		if (!mt_entry->missing_count)
			mt_entry->missing_count++;
		_mapping_add_to_removal_table(sc, mt_entry->dev_handle, 0);
		mt_entry->dev_handle = 0;

		if (((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
		    MPI2_IOCPAGE8_FLAGS_DEVICE_PERSISTENCE_MAPPING) &&
		    sc->is_dpm_enable && !mt_entry->init_complete &&
		    mt_entry->dpm_entry_num != MPR_DPM_BAD_IDX) {
			dpm_entry =
			    (Mpi2DriverMap0Entry_t *) ((u8 *)sc->dpm_pg0 +
			    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
			dpm_entry += mt_entry->dpm_entry_num;
			dpm_entry->MappingInformation = mt_entry->missing_count;
			sc->dpm_flush_entry[mt_entry->dpm_entry_num] = 1;
		}
		mt_entry->init_complete = 1;
	}
}

/**
 * _mapping_find_enc_map_space -find map table entries for enclosure
 * @sc: per adapter object
 * @et_entry: enclosure entry
 *
 * Search through the mapping table defragment it and provide contiguous
 * space in map table for a particular enclosure entry
 *
 * Returns start index in map table or bad index.
 */
static u32
_mapping_find_enc_map_space(struct mpr_softc *sc,
    struct enc_mapping_table *et_entry)
{
	u16 vol_mapping_flags;
	u32 skip_count, end_of_table, map_idx, enc_idx;
	u16 num_found;
	u32 start_idx = MPR_MAPTABLE_BAD_IDX;
	struct dev_mapping_table *mt_entry;
	struct enc_mapping_table *enc_entry;
	unsigned char done_flag = 0, found_space;
	u16 max_num_phy_ids = le16toh(sc->ioc_pg8.MaxNumPhysicalMappedIDs);

	skip_count = sc->num_rsvd_entries;
	num_found = 0;

	vol_mapping_flags = le16toh(sc->ioc_pg8.IRVolumeMappingFlags) &
	    MPI2_IOCPAGE8_IRFLAGS_MASK_VOLUME_MAPPING_MODE;

	if (!sc->ir_firmware)
		end_of_table = sc->max_devices;
	else if (vol_mapping_flags == MPI2_IOCPAGE8_IRFLAGS_LOW_VOLUME_MAPPING)
		end_of_table = sc->max_devices;
	else
		end_of_table = sc->max_devices - sc->max_volumes;

	for (map_idx = (max_num_phy_ids + skip_count);
	    map_idx < end_of_table; map_idx++) {
		mt_entry = &sc->mapping_table[map_idx];
		if ((et_entry->enclosure_id == mt_entry->physical_id) &&
		    (!mt_entry->phy_bits || (mt_entry->phy_bits &
		    et_entry->phy_bits))) {
			num_found += 1;
			if (num_found == et_entry->num_slots) {
				start_idx = (map_idx - num_found) + 1;
				return start_idx;
			}
		} else
			num_found = 0;
	}
	for (map_idx = (max_num_phy_ids + skip_count);
	    map_idx < end_of_table; map_idx++) {
		mt_entry = &sc->mapping_table[map_idx];
		if (!(mt_entry->device_info & MPR_DEV_RESERVED)) {
			num_found += 1;
			if (num_found == et_entry->num_slots) {
				start_idx = (map_idx - num_found) + 1;
				return start_idx;
			}
		} else
			num_found = 0;
	}

	while (!done_flag) {
		enc_idx = _mapping_get_high_missing_et_idx(sc);
		if (enc_idx == MPR_ENCTABLE_BAD_IDX)
			return MPR_MAPTABLE_BAD_IDX;
		enc_entry = &sc->enclosure_table[enc_idx];
		/*VSP FIXME*/
		enc_entry->skip_search = 1;
		mt_entry = &sc->mapping_table[enc_entry->start_index];
		for (map_idx = enc_entry->start_index; map_idx <
		    (enc_entry->start_index + enc_entry->num_slots); map_idx++,
		    mt_entry++)
			mt_entry->device_info  &= ~MPR_DEV_RESERVED;
		found_space = 0;
		for (map_idx = (max_num_phy_ids +
		    skip_count); map_idx < end_of_table; map_idx++) {
			mt_entry = &sc->mapping_table[map_idx];
			if (!(mt_entry->device_info & MPR_DEV_RESERVED)) {
				num_found += 1;
				if (num_found == et_entry->num_slots) {
					start_idx = (map_idx - num_found) + 1;
					found_space = 1;
				}
			} else
				num_found = 0;
		}

		if (!found_space)
			continue;
		for (map_idx = start_idx; map_idx < (start_idx + num_found);
		    map_idx++) {
			enc_entry = sc->enclosure_table;
			for (enc_idx = 0; enc_idx < sc->num_enc_table_entries;
			    enc_idx++, enc_entry++) {
				if (map_idx < enc_entry->start_index ||
				    map_idx > (enc_entry->start_index +
				    enc_entry->num_slots))
					continue;
				if (!enc_entry->removal_flag) {
					enc_entry->removal_flag = 1;
					_mapping_add_to_removal_table(sc, 0,
					    enc_entry->dpm_entry_num);
				}
				mt_entry = &sc->mapping_table[map_idx];
				if (mt_entry->device_info &
				    MPR_MAP_IN_USE) {
					_mapping_add_to_removal_table(sc,
					    mt_entry->dev_handle, 0);
					_mapping_clear_map_entry(mt_entry);
				}
				if (map_idx == (enc_entry->start_index +
				    enc_entry->num_slots - 1))
					_mapping_clear_enc_entry(et_entry);
			}
		}
		enc_entry = sc->enclosure_table;
		for (enc_idx = 0; enc_idx < sc->num_enc_table_entries;
		    enc_idx++, enc_entry++) {
			if (!enc_entry->removal_flag) {
				mt_entry = &sc->mapping_table[enc_entry->
				    start_index];
				for (map_idx = enc_entry->start_index; map_idx <
				    (enc_entry->start_index +
				    enc_entry->num_slots); map_idx++,
				    mt_entry++)
					mt_entry->device_info |=
					    MPR_DEV_RESERVED;
				et_entry->skip_search = 0;
			}
		}
		done_flag = 1;
	}
	return start_idx;
}

/**
 * _mapping_get_dev_info -get information about newly added devices
 * @sc: per adapter object
 * @topo_change: Topology change event entry
 *
 * Search through the topology change event list and issues sas device pg0
 * requests for the newly added device and reserved entries in tables
 *
 * Returns nothing
 */
static void
_mapping_get_dev_info(struct mpr_softc *sc,
    struct _map_topology_change *topo_change)
{
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t sas_device_pg0;
	u8 entry, enc_idx, phy_idx, sata_end_device;
	u32 map_idx, index, device_info;
	struct _map_phy_change *phy_change, *tmp_phy_change;
	uint64_t sas_address;
	struct enc_mapping_table *et_entry;
	struct dev_mapping_table *mt_entry;
	u8 add_code = MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED;
	int rc = 1;

	for (entry = 0; entry < topo_change->num_entries; entry++) {
		phy_change = &topo_change->phy_details[entry];
		if (phy_change->is_processed || !phy_change->dev_handle ||
		    phy_change->reason != MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED)
			continue;
		if (mpr_config_get_sas_device_pg0(sc, &mpi_reply,
		    &sas_device_pg0, MPI2_SAS_DEVICE_PGAD_FORM_HANDLE,
		    phy_change->dev_handle)) {
			phy_change->is_processed = 1;
			continue;
		}

		/*
		 * Always get SATA Identify information because this is used
		 * to determine if Start/Stop Unit should be sent to the drive
		 * when the system is shutdown.
		 */
		device_info = le32toh(sas_device_pg0.DeviceInfo);
		sas_address = sas_device_pg0.SASAddress.High;
		sas_address = (sas_address << 32) |
		    sas_device_pg0.SASAddress.Low;
		sata_end_device = 0;
		if ((device_info & MPI2_SAS_DEVICE_INFO_END_DEVICE) &&
		    (device_info & MPI2_SAS_DEVICE_INFO_SATA_DEVICE)) {
			sata_end_device = 1;
			rc = mprsas_get_sas_address_for_sata_disk(sc,
			    &sas_address, phy_change->dev_handle, device_info,
			    &phy_change->is_SATA_SSD);
			if (rc) {
				mpr_dprint(sc, MPR_ERROR, "%s: failed to get "
				    "disk type (SSD or HDD) and SAS Address "
				    "for SATA device with handle 0x%04x\n",
				    __func__, phy_change->dev_handle);
			} else {
				mpr_dprint(sc, MPR_INFO, "SAS Address for SATA "
				    "device = %jx\n", sas_address);
			}
		}

		phy_change->physical_id = sas_address;
		phy_change->slot = le16toh(sas_device_pg0.Slot);
		phy_change->device_info = le32toh(sas_device_pg0.DeviceInfo);

		if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
		    MPI2_IOCPAGE8_FLAGS_ENCLOSURE_SLOT_MAPPING) {
			enc_idx = _mapping_get_enc_idx_from_handle(sc,
			    topo_change->enc_handle);
			if (enc_idx == MPR_ENCTABLE_BAD_IDX) {
				phy_change->is_processed = 1;
				mpr_dprint(sc, MPR_MAPPING, "%s: failed to add "
				    "the device with handle 0x%04x because the "
				    "enclosure is not in the mapping table\n",
				    __func__, phy_change->dev_handle);
				continue;
			}
			if (!((phy_change->device_info &
			    MPI2_SAS_DEVICE_INFO_END_DEVICE) &&
			    (phy_change->device_info &
			    (MPI2_SAS_DEVICE_INFO_SSP_TARGET |
			    MPI2_SAS_DEVICE_INFO_STP_TARGET |
			    MPI2_SAS_DEVICE_INFO_SATA_DEVICE)))) {
				phy_change->is_processed = 1;
				continue;
			}
			et_entry = &sc->enclosure_table[enc_idx];
			if (et_entry->start_index != MPR_MAPTABLE_BAD_IDX)
				continue;
			if (!topo_change->exp_handle) {
				map_idx	= sc->num_rsvd_entries;
				et_entry->start_index = map_idx;
			} else {
				map_idx = _mapping_find_enc_map_space(sc,
				    et_entry);
				et_entry->start_index = map_idx;
				if (et_entry->start_index ==
				    MPR_MAPTABLE_BAD_IDX) {
					phy_change->is_processed = 1;
					for (phy_idx = 0; phy_idx <
					    topo_change->num_entries;
					    phy_idx++) {
						tmp_phy_change =
						    &topo_change->phy_details
						    [phy_idx];
						if (tmp_phy_change->reason ==
						    add_code)
							tmp_phy_change->
							    is_processed = 1;
					}
					break;
				}
			}
			mt_entry = &sc->mapping_table[map_idx];
			for (index = map_idx; index < (et_entry->num_slots
			    + map_idx); index++, mt_entry++) {
				mt_entry->device_info = MPR_DEV_RESERVED;
				mt_entry->physical_id = et_entry->enclosure_id;
				mt_entry->phy_bits = et_entry->phy_bits;
			}
		}
	}
}

/**
 * _mapping_set_mid_to_eid -set map table data from enclosure table
 * @sc: per adapter object
 * @et_entry: enclosure entry
 *
 * Returns nothing
 */
static inline void
_mapping_set_mid_to_eid(struct mpr_softc *sc,
    struct enc_mapping_table *et_entry)
{
	struct dev_mapping_table *mt_entry;
	u16 slots = et_entry->num_slots, map_idx;
	u32 start_idx = et_entry->start_index;
	if (start_idx != MPR_MAPTABLE_BAD_IDX) {
		mt_entry = &sc->mapping_table[start_idx];
		for (map_idx = 0; map_idx < slots; map_idx++, mt_entry++)
			mt_entry->physical_id = et_entry->enclosure_id;
	}
}

/**
 * _mapping_clear_removed_entries - mark the entries to be cleared
 * @sc: per adapter object
 *
 * Search through the removal table and mark the entries which needs to be
 * flushed to DPM and also updates the map table and enclosure table by
 * clearing the corresponding entries.
 *
 * Returns nothing
 */
static void
_mapping_clear_removed_entries(struct mpr_softc *sc)
{
	u32 remove_idx;
	struct map_removal_table *remove_entry;
	Mpi2DriverMap0Entry_t *dpm_entry;
	u8 done_flag = 0, num_entries, m, i;
	struct enc_mapping_table *et_entry, *from, *to;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);

	if (sc->is_dpm_enable) {
		remove_entry = sc->removal_table;
		for (remove_idx = 0; remove_idx < sc->max_devices;
		    remove_idx++, remove_entry++) {
			if (remove_entry->dpm_entry_num != MPR_DPM_BAD_IDX) {
				dpm_entry = (Mpi2DriverMap0Entry_t *)
				    ((u8 *) sc->dpm_pg0 +
				    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
				dpm_entry += remove_entry->dpm_entry_num;
				dpm_entry->PhysicalIdentifier.Low = 0;
				dpm_entry->PhysicalIdentifier.High = 0;
				dpm_entry->DeviceIndex = 0;
				dpm_entry->MappingInformation = 0;
				dpm_entry->PhysicalBitsMapping = 0;
				sc->dpm_flush_entry[remove_entry->
				    dpm_entry_num] = 1;
				sc->dpm_entry_used[remove_entry->dpm_entry_num]
				    = 0;
				remove_entry->dpm_entry_num = MPR_DPM_BAD_IDX;
			}
		}
	}
	if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
	    MPI2_IOCPAGE8_FLAGS_ENCLOSURE_SLOT_MAPPING) {
		num_entries = sc->num_enc_table_entries;
		while (!done_flag) {
			done_flag = 1;
			et_entry = sc->enclosure_table;
			for (i = 0; i < num_entries; i++, et_entry++) {
				if (!et_entry->enc_handle && et_entry->
				    init_complete) {
					done_flag = 0;
					if (i != (num_entries - 1)) {
						from = &sc->enclosure_table
						    [i+1];
						to = &sc->enclosure_table[i];
						for (m = i; m < (num_entries -
						    1); m++, from++, to++) {
							_mapping_set_mid_to_eid
							    (sc, to);
							*to = *from;
						}
						_mapping_clear_enc_entry(to);
						sc->num_enc_table_entries--;
						num_entries =
						    sc->num_enc_table_entries;
					} else {
						_mapping_clear_enc_entry
						    (et_entry);
						sc->num_enc_table_entries--;
						num_entries =
						    sc->num_enc_table_entries;
					}
				}
			}
		}
	}
}

/**
 * _mapping_add_new_device -Add the new device into mapping table
 * @sc: per adapter object
 * @topo_change: Topology change event entry
 *
 * Search through the topology change event list and updates map table,
 * enclosure table and DPM pages for for the newly added devices.
 *
 * Returns nothing
 */
static void
_mapping_add_new_device(struct mpr_softc *sc,
    struct _map_topology_change *topo_change)
{
	u8 enc_idx, missing_cnt, is_removed = 0;
	u16 dpm_idx;
	u32 search_idx, map_idx;
	u32 entry;
	struct dev_mapping_table *mt_entry;
	struct enc_mapping_table *et_entry;
	struct _map_phy_change *phy_change;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);
	Mpi2DriverMap0Entry_t *dpm_entry;
	uint64_t temp64_var;
	u8 map_shift = MPI2_DRVMAP0_MAPINFO_SLOT_SHIFT;
	u8 hdr_sz = sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER);
	u16 max_num_phy_ids = le16toh(sc->ioc_pg8.MaxNumPhysicalMappedIDs);

	for (entry = 0; entry < topo_change->num_entries; entry++) {
		phy_change = &topo_change->phy_details[entry];
		if (phy_change->is_processed)
			continue;
		if (phy_change->reason != MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED ||
		    !phy_change->dev_handle) {
			phy_change->is_processed = 1;
			continue;
		}
		if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
		    MPI2_IOCPAGE8_FLAGS_ENCLOSURE_SLOT_MAPPING) {
			enc_idx = _mapping_get_enc_idx_from_handle
			    (sc, topo_change->enc_handle);
			if (enc_idx == MPR_ENCTABLE_BAD_IDX) {
				phy_change->is_processed = 1;
				printf("%s: failed to add the device with "
				    "handle 0x%04x because the enclosure is "
				    "not in the mapping table\n", __func__,
				    phy_change->dev_handle);
				continue;
			}
			et_entry = &sc->enclosure_table[enc_idx];
			if (et_entry->start_index == MPR_MAPTABLE_BAD_IDX) {
				phy_change->is_processed = 1;
				if (!sc->mt_full_retry) {
					sc->mt_add_device_failed = 1;
					continue;
				}
				printf("%s: failed to add the device with "
				    "handle 0x%04x because there is no free "
				    "space available in the mapping table\n",
				    __func__, phy_change->dev_handle);
				continue;
			}
			map_idx = et_entry->start_index + phy_change->slot -
			    et_entry->start_slot;
			mt_entry = &sc->mapping_table[map_idx];
			mt_entry->physical_id = phy_change->physical_id;
			mt_entry->channel = 0;
			mt_entry->id = map_idx;
			mt_entry->dev_handle = phy_change->dev_handle;
			mt_entry->missing_count = 0;
			mt_entry->dpm_entry_num = et_entry->dpm_entry_num;
			mt_entry->device_info = phy_change->device_info |
			    (MPR_DEV_RESERVED | MPR_MAP_IN_USE);
			if (sc->is_dpm_enable) {
				dpm_idx = et_entry->dpm_entry_num;
				if (dpm_idx == MPR_DPM_BAD_IDX)
					dpm_idx = _mapping_get_dpm_idx_from_id
					    (sc, et_entry->enclosure_id,
					     et_entry->phy_bits);
				if (dpm_idx == MPR_DPM_BAD_IDX) {
					dpm_idx = _mapping_get_free_dpm_idx(sc);
					if (dpm_idx != MPR_DPM_BAD_IDX) {
						dpm_entry =
						    (Mpi2DriverMap0Entry_t *)
						    ((u8 *) sc->dpm_pg0 +
						     hdr_sz);
						dpm_entry += dpm_idx;
						dpm_entry->
						    PhysicalIdentifier.Low =
						    (0xFFFFFFFF &
						    et_entry->enclosure_id);
						dpm_entry->
						    PhysicalIdentifier.High =
						    ( et_entry->enclosure_id
						     >> 32);
						dpm_entry->DeviceIndex =
						    (U16)et_entry->start_index;
						dpm_entry->MappingInformation =
							et_entry->num_slots;
						dpm_entry->MappingInformation
						    <<= map_shift;
						dpm_entry->PhysicalBitsMapping
						    = et_entry->phy_bits;
						et_entry->dpm_entry_num =
						    dpm_idx;
		/* FIXME Do I need to set the dpm_idxin mt_entry too */
						sc->dpm_entry_used[dpm_idx] = 1;
						sc->dpm_flush_entry[dpm_idx] =
						    1;
						phy_change->is_processed = 1;
					} else {
						phy_change->is_processed = 1;
						mpr_dprint(sc, MPR_INFO, "%s: "
						    "failed to add the device "
						    "with handle 0x%04x to "
						    "persistent table because "
						    "there is no free space "
						    "available\n", __func__,
						    phy_change->dev_handle);
					}
				} else {
					et_entry->dpm_entry_num = dpm_idx;
					mt_entry->dpm_entry_num = dpm_idx;
				}
			}
			/* FIXME Why not mt_entry too? */
			et_entry->init_complete = 1;
		} else if ((ioc_pg8_flags &
		    MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
		    MPI2_IOCPAGE8_FLAGS_DEVICE_PERSISTENCE_MAPPING) {
			map_idx = _mapping_get_mt_idx_from_id
			    (sc, phy_change->physical_id);
			if (map_idx == MPR_MAPTABLE_BAD_IDX) {
				search_idx = sc->num_rsvd_entries;
				if (topo_change->exp_handle)
					search_idx += max_num_phy_ids;
				map_idx = _mapping_get_free_mt_idx(sc,
				    search_idx);
			}
			if (map_idx == MPR_MAPTABLE_BAD_IDX) {
				map_idx = _mapping_get_high_missing_mt_idx(sc);
				if (map_idx != MPR_MAPTABLE_BAD_IDX) {
					mt_entry = &sc->mapping_table[map_idx];
					if (mt_entry->dev_handle) {
						_mapping_add_to_removal_table
						    (sc, mt_entry->dev_handle,
						     0);
						is_removed = 1;
					}
					mt_entry->init_complete = 0;
				}
			}
			if (map_idx != MPR_MAPTABLE_BAD_IDX) {
				mt_entry = &sc->mapping_table[map_idx];
				mt_entry->physical_id = phy_change->physical_id;
				mt_entry->channel = 0;
				mt_entry->id = map_idx;
				mt_entry->dev_handle = phy_change->dev_handle;
				mt_entry->missing_count = 0;
				mt_entry->device_info = phy_change->device_info
				    | (MPR_DEV_RESERVED | MPR_MAP_IN_USE);
			} else {
				phy_change->is_processed = 1;
				if (!sc->mt_full_retry) {
					sc->mt_add_device_failed = 1;
					continue;
				}
				printf("%s: failed to add the device with "
				    "handle 0x%04x because there is no free "
				    "space available in the mapping table\n",
				    __func__, phy_change->dev_handle);
				continue;
			}
			if (sc->is_dpm_enable) {
				if (mt_entry->dpm_entry_num !=
				    MPR_DPM_BAD_IDX) {
					dpm_idx = mt_entry->dpm_entry_num;
					dpm_entry = (Mpi2DriverMap0Entry_t *)
					    ((u8 *)sc->dpm_pg0 + hdr_sz);
					dpm_entry += dpm_idx;
					missing_cnt = dpm_entry->
					    MappingInformation &
					    MPI2_DRVMAP0_MAPINFO_MISSING_MASK;
					temp64_var = dpm_entry->
					    PhysicalIdentifier.High;
					temp64_var = (temp64_var << 32) |
					   dpm_entry->PhysicalIdentifier.Low;
					if ((mt_entry->physical_id ==
					    temp64_var) && !missing_cnt)
						mt_entry->init_complete = 1;
				} else {
					dpm_idx = _mapping_get_free_dpm_idx(sc);
					mt_entry->init_complete = 0;
				}
				if (dpm_idx != MPR_DPM_BAD_IDX &&
				    !mt_entry->init_complete) {
					mt_entry->init_complete = 1;
					mt_entry->dpm_entry_num = dpm_idx;
					dpm_entry = (Mpi2DriverMap0Entry_t *)
					    ((u8 *)sc->dpm_pg0 + hdr_sz);
					dpm_entry += dpm_idx;
					dpm_entry->PhysicalIdentifier.Low =
					    (0xFFFFFFFF &
					    mt_entry->physical_id);
					dpm_entry->PhysicalIdentifier.High =
					    (mt_entry->physical_id >> 32);
					dpm_entry->DeviceIndex = (U16) map_idx;
					dpm_entry->MappingInformation = 0;
					dpm_entry->PhysicalBitsMapping = 0;
					sc->dpm_entry_used[dpm_idx] = 1;
					sc->dpm_flush_entry[dpm_idx] = 1;
					phy_change->is_processed = 1;
				} else if (dpm_idx == MPR_DPM_BAD_IDX) {
						phy_change->is_processed = 1;
						mpr_dprint(sc, MPR_INFO, "%s: "
						    "failed to add the device "
						    "with handle 0x%04x to "
						    "persistent table because "
						    "there is no free space "
						    "available\n", __func__,
						    phy_change->dev_handle);
				}
			}
			mt_entry->init_complete = 1;
		}

		phy_change->is_processed = 1;
	}
	if (is_removed)
		_mapping_clear_removed_entries(sc);
}

/**
 * _mapping_flush_dpm_pages -Flush the DPM pages to NVRAM
 * @sc: per adapter object
 *
 * Returns nothing
 */
static void
_mapping_flush_dpm_pages(struct mpr_softc *sc)
{
	Mpi2DriverMap0Entry_t *dpm_entry;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2DriverMappingPage0_t config_page;
	u16 entry_num;

	for (entry_num = 0; entry_num < sc->max_dpm_entries; entry_num++) {
		if (!sc->dpm_flush_entry[entry_num])
			continue;
		memset(&config_page, 0, sizeof(Mpi2DriverMappingPage0_t));
		memcpy(&config_page.Header, (u8 *)sc->dpm_pg0,
		    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
		dpm_entry = (Mpi2DriverMap0Entry_t *) ((u8 *)sc->dpm_pg0 +
		    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
		dpm_entry += entry_num;
		dpm_entry->MappingInformation = htole16(dpm_entry->
		    MappingInformation);
		dpm_entry->DeviceIndex = htole16(dpm_entry->DeviceIndex);
		dpm_entry->PhysicalBitsMapping = htole32(dpm_entry->
		    PhysicalBitsMapping);
		memcpy(&config_page.Entry, (u8 *)dpm_entry,
		    sizeof(Mpi2DriverMap0Entry_t));
		/* TODO-How to handle failed writes? */
		if (mpr_config_set_dpm_pg0(sc, &mpi_reply, &config_page,
		    entry_num)) {
			printf("%s: write of dpm entry %d for device failed\n",
			     __func__, entry_num);
		} else
			sc->dpm_flush_entry[entry_num] = 0;
		dpm_entry->MappingInformation = le16toh(dpm_entry->
		    MappingInformation);
		dpm_entry->DeviceIndex = le16toh(dpm_entry->DeviceIndex);
		dpm_entry->PhysicalBitsMapping = le32toh(dpm_entry->
		    PhysicalBitsMapping);
	}
}

/**
 * _mapping_allocate_memory- allocates the memory required for mapping tables
 * @sc: per adapter object
 *
 * Allocates the memory for all the tables required for host mapping
 *
 * Return 0 on success or non-zero on failure.
 */
int
mpr_mapping_allocate_memory(struct mpr_softc *sc)
{
	uint32_t dpm_pg0_sz;

	sc->mapping_table = malloc((sizeof(struct dev_mapping_table) *
	    sc->max_devices), M_MPR, M_ZERO|M_NOWAIT);
	if (!sc->mapping_table)
		goto free_resources;

	sc->removal_table = malloc((sizeof(struct map_removal_table) *
	    sc->max_devices), M_MPR, M_ZERO|M_NOWAIT);
	if (!sc->removal_table)
		goto free_resources;

	sc->enclosure_table = malloc((sizeof(struct enc_mapping_table) *
	    sc->max_enclosures), M_MPR, M_ZERO|M_NOWAIT);
	if (!sc->enclosure_table)
		goto free_resources;

	sc->dpm_entry_used = malloc((sizeof(u8) * sc->max_dpm_entries),
	    M_MPR, M_ZERO|M_NOWAIT);
	if (!sc->dpm_entry_used)
		goto free_resources;

	sc->dpm_flush_entry = malloc((sizeof(u8) * sc->max_dpm_entries),
	    M_MPR, M_ZERO|M_NOWAIT);
	if (!sc->dpm_flush_entry)
		goto free_resources;

	dpm_pg0_sz = sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER) +
	    (sc->max_dpm_entries * sizeof(MPI2_CONFIG_PAGE_DRIVER_MAP0_ENTRY));

	sc->dpm_pg0 = malloc(dpm_pg0_sz, M_MPR, M_ZERO|M_NOWAIT);
	if (!sc->dpm_pg0) {
		printf("%s: memory alloc failed for dpm page; disabling dpm\n",
		    __func__);
		sc->is_dpm_enable = 0;
	}

	return 0;

free_resources:
	free(sc->mapping_table, M_MPR);
	free(sc->removal_table, M_MPR);
	free(sc->enclosure_table, M_MPR);
	free(sc->dpm_entry_used, M_MPR);
	free(sc->dpm_flush_entry, M_MPR);
	free(sc->dpm_pg0, M_MPR);
	printf("%s: device initialization failed due to failure in mapping "
	    "table memory allocation\n", __func__);
	return -1;
}

/**
 * mpr_mapping_free_memory- frees the memory allocated for mapping tables
 * @sc: per adapter object
 *
 * Returns nothing.
 */
void
mpr_mapping_free_memory(struct mpr_softc *sc)
{
	free(sc->mapping_table, M_MPR);
	free(sc->removal_table, M_MPR);
	free(sc->enclosure_table, M_MPR);
	free(sc->dpm_entry_used, M_MPR);
	free(sc->dpm_flush_entry, M_MPR);
	free(sc->dpm_pg0, M_MPR);
}


static void
_mapping_process_dpm_pg0(struct mpr_softc *sc)
{
	u8 missing_cnt, enc_idx;
	u16 slot_id, entry_num, num_slots;
	u32 map_idx, dev_idx, start_idx, end_idx;
	struct dev_mapping_table *mt_entry;
	Mpi2DriverMap0Entry_t *dpm_entry;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);
	u16 max_num_phy_ids = le16toh(sc->ioc_pg8.MaxNumPhysicalMappedIDs);
	struct enc_mapping_table *et_entry;
	u64 physical_id;
	u32 phy_bits = 0;

	if (sc->ir_firmware)
		_mapping_get_ir_maprange(sc, &start_idx, &end_idx);

	dpm_entry = (Mpi2DriverMap0Entry_t *) ((uint8_t *) sc->dpm_pg0 +
	    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
	for (entry_num = 0; entry_num < sc->max_dpm_entries; entry_num++, 
	    dpm_entry++) {
		physical_id = dpm_entry->PhysicalIdentifier.High;
		physical_id = (physical_id << 32) | 
		    dpm_entry->PhysicalIdentifier.Low;
		if (!physical_id) {
			sc->dpm_entry_used[entry_num] = 0;
			continue;
		}
		sc->dpm_entry_used[entry_num] = 1;
		dpm_entry->MappingInformation = le16toh(dpm_entry->
		    MappingInformation);
		missing_cnt = dpm_entry->MappingInformation &
		    MPI2_DRVMAP0_MAPINFO_MISSING_MASK;
		dev_idx = le16toh(dpm_entry->DeviceIndex);
		phy_bits = le32toh(dpm_entry->PhysicalBitsMapping);
		if (sc->ir_firmware && (dev_idx >= start_idx) &&
		    (dev_idx <= end_idx)) {
			mt_entry = &sc->mapping_table[dev_idx];
			mt_entry->physical_id = dpm_entry->PhysicalIdentifier.High;
			mt_entry->physical_id = (mt_entry->physical_id << 32) |
			    dpm_entry->PhysicalIdentifier.Low;
			mt_entry->channel = MPR_RAID_CHANNEL;
			mt_entry->id = dev_idx;
			mt_entry->missing_count = missing_cnt;
			mt_entry->dpm_entry_num = entry_num;
			mt_entry->device_info = MPR_DEV_RESERVED;
			continue;
		}
		if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
		    MPI2_IOCPAGE8_FLAGS_ENCLOSURE_SLOT_MAPPING) {
			if (dev_idx <  (sc->num_rsvd_entries +
			    max_num_phy_ids)) {
				slot_id = 0;
				if (ioc_pg8_flags &
				    MPI2_IOCPAGE8_FLAGS_DA_START_SLOT_1)
					slot_id = 1;
				num_slots = max_num_phy_ids;
			} else {
				slot_id = 0;
				num_slots = dpm_entry->MappingInformation &
				    MPI2_DRVMAP0_MAPINFO_SLOT_MASK;
				num_slots >>= MPI2_DRVMAP0_MAPINFO_SLOT_SHIFT;
			}
			enc_idx = sc->num_enc_table_entries;
			if (enc_idx >= sc->max_enclosures) {
				printf("%s: enclosure entries exceed max "
				    "enclosures of %d\n", __func__,
				    sc->max_enclosures);
				break;
			}
			sc->num_enc_table_entries++;
			et_entry = &sc->enclosure_table[enc_idx];
			physical_id = dpm_entry->PhysicalIdentifier.High;
			et_entry->enclosure_id = (physical_id << 32) |
			    dpm_entry->PhysicalIdentifier.Low;
			et_entry->start_index = dev_idx;
			et_entry->dpm_entry_num = entry_num;
			et_entry->num_slots = num_slots;
			et_entry->start_slot = slot_id;
			et_entry->missing_count = missing_cnt;
			et_entry->phy_bits = phy_bits;

			mt_entry = &sc->mapping_table[dev_idx];
			for (map_idx = dev_idx; map_idx < (dev_idx + num_slots);
			    map_idx++, mt_entry++) {
				if (mt_entry->dpm_entry_num !=
				    MPR_DPM_BAD_IDX) {
					printf("%s: conflict in mapping table "
					    "for enclosure %d\n", __func__,
					    enc_idx);
					break;
				}
				physical_id = dpm_entry->PhysicalIdentifier.High;
				mt_entry->physical_id = (physical_id << 32) |
				    dpm_entry->PhysicalIdentifier.Low;
				mt_entry->phy_bits = phy_bits;
				mt_entry->channel = 0;
				mt_entry->id = dev_idx;
				mt_entry->dpm_entry_num = entry_num;
				mt_entry->missing_count = missing_cnt;
				mt_entry->device_info = MPR_DEV_RESERVED;
			}
		} else if ((ioc_pg8_flags &
		    MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
		    MPI2_IOCPAGE8_FLAGS_DEVICE_PERSISTENCE_MAPPING) {
			map_idx = dev_idx;
			mt_entry = &sc->mapping_table[map_idx];
			if (mt_entry->dpm_entry_num != MPR_DPM_BAD_IDX) {
				printf("%s: conflict in mapping table for "
				    "device %d\n", __func__, map_idx);
				break;
			}
			physical_id = dpm_entry->PhysicalIdentifier.High;
			mt_entry->physical_id = (physical_id << 32) |
			    dpm_entry->PhysicalIdentifier.Low;
			mt_entry->phy_bits = phy_bits;
			mt_entry->channel = 0;
			mt_entry->id = dev_idx;
			mt_entry->missing_count = missing_cnt;
			mt_entry->dpm_entry_num = entry_num;
			mt_entry->device_info = MPR_DEV_RESERVED;
		}
	} /*close the loop for DPM table */
}

/*
 * mpr_mapping_check_devices - start of the day check for device availabilty
 * @sc: per adapter object
 * @sleep_flag: Flag indicating whether this function can sleep or not
 *
 * Returns nothing.
 */
void
mpr_mapping_check_devices(struct mpr_softc *sc, int sleep_flag)
{
	u32 i;
/*	u32 cntdn, i;
	u32 timeout = 60;*/
	struct dev_mapping_table *mt_entry;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);
	struct enc_mapping_table *et_entry;
	u32 start_idx, end_idx;

	/* We need to ucomment this when this function is called
	 * from the port enable complete */
#if 0
	sc->track_mapping_events = 0;
	cntdn = (sleep_flag == CAN_SLEEP) ? 1000*timeout : 2000*timeout;
	do {
		if (!sc->pending_map_events)
			break;
		if (sleep_flag == CAN_SLEEP)
			pause("mpr_pause", (hz/1000));/* 1msec sleep */
		else
			DELAY(500); /* 500 useconds delay */
	} while (--cntdn);


	if (!cntdn)
		printf("%s: there are %d"
		    " pending events after %d seconds of delay\n",
		    __func__, sc->pending_map_events, timeout);
#endif
	sc->pending_map_events = 0;

	if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
	    MPI2_IOCPAGE8_FLAGS_ENCLOSURE_SLOT_MAPPING) {
		et_entry = sc->enclosure_table;
		for (i = 0; i < sc->num_enc_table_entries; i++, et_entry++) {
			if (!et_entry->init_complete) {
				if (et_entry->missing_count <
				    MPR_MAX_MISSING_COUNT) {
					et_entry->missing_count++;
					if (et_entry->dpm_entry_num !=
					    MPR_DPM_BAD_IDX)
						_mapping_commit_enc_entry(sc,
						    et_entry);
				}
				et_entry->init_complete = 1;
			}
		}
		if (!sc->ir_firmware)
			return;
		_mapping_get_ir_maprange(sc, &start_idx, &end_idx);
		mt_entry = &sc->mapping_table[start_idx];
		for (i = start_idx; i < (end_idx + 1); i++, mt_entry++) {
			if (mt_entry->device_info & MPR_DEV_RESERVED
			    && !mt_entry->physical_id)
				mt_entry->init_complete = 1;
			else if (mt_entry->device_info & MPR_DEV_RESERVED) {
				if (!mt_entry->init_complete) {
					if (mt_entry->missing_count <
					    MPR_MAX_MISSING_COUNT) {
						mt_entry->missing_count++;
						if (mt_entry->dpm_entry_num !=
						    MPR_DPM_BAD_IDX)
						_mapping_commit_map_entry(sc,
						    mt_entry);
					}
					mt_entry->init_complete = 1;
				}
			}
		}
	} else if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) ==
	    MPI2_IOCPAGE8_FLAGS_DEVICE_PERSISTENCE_MAPPING) {
		mt_entry = sc->mapping_table;
		for (i = 0; i < sc->max_devices; i++, mt_entry++) {
			if (mt_entry->device_info & MPR_DEV_RESERVED
			    && !mt_entry->physical_id)
				mt_entry->init_complete = 1;
			else if (mt_entry->device_info & MPR_DEV_RESERVED) {
				if (!mt_entry->init_complete) {
					if (mt_entry->missing_count <
					    MPR_MAX_MISSING_COUNT) {
						mt_entry->missing_count++;
						if (mt_entry->dpm_entry_num !=
						    MPR_DPM_BAD_IDX)
						_mapping_commit_map_entry(sc,
						    mt_entry);
					}
					mt_entry->init_complete = 1;
				}
			}
		}
	}
}


/**
 * mpr_mapping_is_reinit_required - check whether event replay required
 * @sc: per adapter object
 *
 * Checks the per ioc flags and decide whether reinit of events required
 *
 * Returns 1 for reinit of ioc 0 for not.
 */
int mpr_mapping_is_reinit_required(struct mpr_softc *sc)
{
	if (!sc->mt_full_retry && sc->mt_add_device_failed) {
		sc->mt_full_retry = 1;
		sc->mt_add_device_failed = 0;
		_mapping_flush_dpm_pages(sc);
		return 1;
	}
	sc->mt_full_retry = 1;
	return 0;
}

/**
 * mpr_mapping_initialize - initialize mapping tables
 * @sc: per adapter object
 *
 * Read controller persitant mapping tables into internal data area.
 *
 * Return 0 for success or non-zero for failure.
 */
int
mpr_mapping_initialize(struct mpr_softc *sc)
{
	uint16_t volume_mapping_flags, dpm_pg0_sz;
	uint32_t i;
	Mpi2ConfigReply_t mpi_reply;
	int error;
	uint8_t retry_count;
	uint16_t ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);

	/* The additional 1 accounts for the virtual enclosure
	 * created for the controller
	 */
	sc->max_enclosures = sc->facts->MaxEnclosures + 1;
	sc->max_expanders = sc->facts->MaxSasExpanders;
	sc->max_volumes = sc->facts->MaxVolumes;
	sc->max_devices = sc->facts->MaxTargets + sc->max_volumes;
	sc->pending_map_events = 0;
	sc->num_enc_table_entries = 0;
	sc->num_rsvd_entries = 0;
	sc->num_channels = 1;
	sc->max_dpm_entries = sc->ioc_pg8.MaxPersistentEntries;
	sc->is_dpm_enable = (sc->max_dpm_entries) ? 1 : 0;
	sc->track_mapping_events = 0;
	
	if (ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_DISABLE_PERSISTENT_MAPPING)
		sc->is_dpm_enable = 0;

	if (ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_RESERVED_TARGETID_0)
		sc->num_rsvd_entries = 1;

	volume_mapping_flags = sc->ioc_pg8.IRVolumeMappingFlags &
	    MPI2_IOCPAGE8_IRFLAGS_MASK_VOLUME_MAPPING_MODE;
	if (sc->ir_firmware && (volume_mapping_flags ==
	    MPI2_IOCPAGE8_IRFLAGS_LOW_VOLUME_MAPPING))
		sc->num_rsvd_entries += sc->max_volumes;

	error = mpr_mapping_allocate_memory(sc);
	if (error)
		return (error);

	for (i = 0; i < sc->max_devices; i++)
		_mapping_clear_map_entry(sc->mapping_table + i);

	for (i = 0; i < sc->max_enclosures; i++)
		_mapping_clear_enc_entry(sc->enclosure_table + i);

	for (i = 0; i < sc->max_devices; i++) {
		sc->removal_table[i].dev_handle = 0;
		sc->removal_table[i].dpm_entry_num = MPR_DPM_BAD_IDX;
	}

	memset(sc->dpm_entry_used, 0, sc->max_dpm_entries);
	memset(sc->dpm_flush_entry, 0, sc->max_dpm_entries);

	if (sc->is_dpm_enable) {
		dpm_pg0_sz = sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER) +
		    (sc->max_dpm_entries *
		     sizeof(MPI2_CONFIG_PAGE_DRIVER_MAP0_ENTRY));
		retry_count = 0;

retry_read_dpm:
		if (mpr_config_get_dpm_pg0(sc, &mpi_reply, sc->dpm_pg0,
		    dpm_pg0_sz)) {
			printf("%s: dpm page read failed; disabling dpm\n",
			    __func__);
			if (retry_count < 3) {
				retry_count++;
				goto retry_read_dpm;
			}
			sc->is_dpm_enable = 0;
		}
	}

	if (sc->is_dpm_enable)
		_mapping_process_dpm_pg0(sc);

	sc->track_mapping_events = 1;
	return 0;
}

/**
 * mpr_mapping_exit - clear mapping table and associated memory
 * @sc: per adapter object
 *
 * Returns nothing.
 */
void
mpr_mapping_exit(struct mpr_softc *sc)
{
	_mapping_flush_dpm_pages(sc);
	mpr_mapping_free_memory(sc);
}

/**
 * mpr_mapping_get_sas_id - assign a target id for sas device
 * @sc: per adapter object
 * @sas_address: sas address of the device
 * @handle: device handle
 *
 * Returns valid ID on success or BAD_ID.
 */
unsigned int
mpr_mapping_get_sas_id(struct mpr_softc *sc, uint64_t sas_address, u16 handle)
{
	u32 map_idx;
	struct dev_mapping_table *mt_entry;

	for (map_idx = 0; map_idx < sc->max_devices; map_idx++) {
		mt_entry = &sc->mapping_table[map_idx];
		if (mt_entry->dev_handle == handle && mt_entry->physical_id ==
		    sas_address)
			return mt_entry->id;
	}

	return MPR_MAP_BAD_ID;
}

/**
 * mpr_mapping_get_sas_id_from_handle - find a target id in mapping table using
 * only the dev handle.  This is just a wrapper function for the local function
 * _mapping_get_mt_idx_from_handle.
 * @sc: per adapter object
 * @handle: device handle
 *
 * Returns valid ID on success or BAD_ID.
 */
unsigned int
mpr_mapping_get_sas_id_from_handle(struct mpr_softc *sc, u16 handle)
{
	return (_mapping_get_mt_idx_from_handle(sc, handle));
}

/**
 * mpr_mapping_get_raid_id - assign a target id for raid device
 * @sc: per adapter object
 * @wwid: world wide identifier for raid volume
 * @handle: device handle
 *
 * Returns valid ID on success or BAD_ID.
 */
unsigned int
mpr_mapping_get_raid_id(struct mpr_softc *sc, u64 wwid, u16 handle)
{
	u32 map_idx;
	struct dev_mapping_table *mt_entry;

	for (map_idx = 0; map_idx < sc->max_devices; map_idx++) {
		mt_entry = &sc->mapping_table[map_idx];
		if (mt_entry->dev_handle == handle && mt_entry->physical_id ==
		    wwid)
			return mt_entry->id;
	}

	return MPR_MAP_BAD_ID;
}

/**
 * mpr_mapping_get_raid_id_from_handle - find raid device in mapping table
 * using only the volume dev handle.  This is just a wrapper function for the
 * local function _mapping_get_ir_mt_idx_from_handle.
 * @sc: per adapter object
 * @volHandle: volume device handle
 *
 * Returns valid ID on success or BAD_ID.
 */
unsigned int
mpr_mapping_get_raid_id_from_handle(struct mpr_softc *sc, u16 volHandle)
{
	return (_mapping_get_ir_mt_idx_from_handle(sc, volHandle));
}

/**
 * mpr_mapping_enclosure_dev_status_change_event - handle enclosure events
 * @sc: per adapter object
 * @event_data: event data payload
 *
 * Return nothing.
 */
void
mpr_mapping_enclosure_dev_status_change_event(struct mpr_softc *sc,
    Mpi2EventDataSasEnclDevStatusChange_t *event_data)
{
	u8 enc_idx, missing_count;
	struct enc_mapping_table *et_entry;
	Mpi2DriverMap0Entry_t *dpm_entry;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);
	u8 map_shift = MPI2_DRVMAP0_MAPINFO_SLOT_SHIFT;
	u8 update_phy_bits = 0;
	u32 saved_phy_bits;
	uint64_t temp64_var;

	if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE) !=
	    MPI2_IOCPAGE8_FLAGS_ENCLOSURE_SLOT_MAPPING)
		goto out;

	dpm_entry = (Mpi2DriverMap0Entry_t *)((u8 *)sc->dpm_pg0 +
	    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));

	if (event_data->ReasonCode == MPI2_EVENT_SAS_ENCL_RC_ADDED) {
		if (!event_data->NumSlots) {
			printf("%s: enclosure with handle = 0x%x reported 0 "
			    "slots\n", __func__,
			    le16toh(event_data->EnclosureHandle));
			goto out;
		}
		temp64_var = event_data->EnclosureLogicalID.High;
		temp64_var = (temp64_var << 32) |
		    event_data->EnclosureLogicalID.Low;
		enc_idx = _mapping_get_enc_idx_from_id(sc, temp64_var,
		    event_data->PhyBits);
		if (enc_idx != MPR_ENCTABLE_BAD_IDX) {
			et_entry = &sc->enclosure_table[enc_idx];
			if (et_entry->init_complete &&
			    !et_entry->missing_count) {
				printf("%s: enclosure %d is already present "
				    "with handle = 0x%x\n",__func__, enc_idx,
				    et_entry->enc_handle);
				goto out;
			}
			et_entry->enc_handle = le16toh(event_data->
			    EnclosureHandle);
			et_entry->start_slot = le16toh(event_data->StartSlot);
			saved_phy_bits = et_entry->phy_bits;
			et_entry->phy_bits |= le32toh(event_data->PhyBits);
			if (saved_phy_bits != et_entry->phy_bits)
				update_phy_bits = 1;
			if (et_entry->missing_count || update_phy_bits) {
				et_entry->missing_count = 0;
				if (sc->is_dpm_enable &&
				    et_entry->dpm_entry_num !=
				    MPR_DPM_BAD_IDX) {
					dpm_entry += et_entry->dpm_entry_num;
					missing_count =
					    (u8)(dpm_entry->MappingInformation &
					    MPI2_DRVMAP0_MAPINFO_MISSING_MASK);
					if (!et_entry->init_complete && (
					    missing_count || update_phy_bits)) {
						dpm_entry->MappingInformation
						    = et_entry->num_slots;
						dpm_entry->MappingInformation
						    <<= map_shift;
						dpm_entry->PhysicalBitsMapping
						    = et_entry->phy_bits;
						sc->dpm_flush_entry[et_entry->
						    dpm_entry_num] = 1;
					}
				}
			}
		} else {
			enc_idx = sc->num_enc_table_entries;
			if (enc_idx >= sc->max_enclosures) {
				printf("%s: enclosure can not be added; "
				    "mapping table is full\n", __func__);
				goto out;
			}
			sc->num_enc_table_entries++;
			et_entry = &sc->enclosure_table[enc_idx];
			et_entry->enc_handle = le16toh(event_data->
			    EnclosureHandle);
			et_entry->enclosure_id = event_data->
			    EnclosureLogicalID.High;
			et_entry->enclosure_id = ( et_entry->enclosure_id << 
			    32) | event_data->EnclosureLogicalID.Low;
			et_entry->start_index = MPR_MAPTABLE_BAD_IDX;
			et_entry->dpm_entry_num = MPR_DPM_BAD_IDX;
			et_entry->num_slots = le16toh(event_data->NumSlots);
			et_entry->start_slot = le16toh(event_data->StartSlot);
			et_entry->phy_bits = le32toh(event_data->PhyBits);
		}
		et_entry->init_complete = 1;
	} else if (event_data->ReasonCode ==
	    MPI2_EVENT_SAS_ENCL_RC_NOT_RESPONDING) {
		enc_idx = _mapping_get_enc_idx_from_handle(sc,
		    le16toh(event_data->EnclosureHandle));
		if (enc_idx == MPR_ENCTABLE_BAD_IDX) {
			printf("%s: cannot unmap enclosure %d because it has "
			    "already been deleted", __func__, enc_idx);
			goto out;
		}
		et_entry = &sc->enclosure_table[enc_idx];
		if (!et_entry->init_complete) {
			if (et_entry->missing_count < MPR_MAX_MISSING_COUNT)
				et_entry->missing_count++;
			else
				et_entry->init_complete = 1;
		}
		if (!et_entry->missing_count)
			et_entry->missing_count++;
		if (sc->is_dpm_enable && !et_entry->init_complete &&
		    et_entry->dpm_entry_num != MPR_DPM_BAD_IDX) {
			dpm_entry += et_entry->dpm_entry_num;
			dpm_entry->MappingInformation = et_entry->num_slots;
			dpm_entry->MappingInformation <<= map_shift;
			dpm_entry->MappingInformation |=
			    et_entry->missing_count;
			sc->dpm_flush_entry[et_entry->dpm_entry_num] = 1;
		}
		et_entry->init_complete = 1;
	}

out:
	_mapping_flush_dpm_pages(sc);
	if (sc->pending_map_events)
		sc->pending_map_events--;
}

/**
 * mpr_mapping_topology_change_event - handle topology change events
 * @sc: per adapter object
 * @event_data: event data payload
 *
 * Returns nothing.
 */
void
mpr_mapping_topology_change_event(struct mpr_softc *sc,
    Mpi2EventDataSasTopologyChangeList_t *event_data)
{
	struct _map_topology_change topo_change;
	struct _map_phy_change *phy_change;
	Mpi2EventSasTopoPhyEntry_t *event_phy_change;
	u8 i, num_entries;

	topo_change.enc_handle = le16toh(event_data->EnclosureHandle);
	topo_change.exp_handle = le16toh(event_data->ExpanderDevHandle);
	num_entries = event_data->NumEntries;
	topo_change.num_entries = num_entries;
	topo_change.start_phy_num = event_data->StartPhyNum;
	topo_change.num_phys = event_data->NumPhys;
	topo_change.exp_status = event_data->ExpStatus;
	event_phy_change = event_data->PHY;
	topo_change.phy_details = NULL;

	if (!num_entries)
		goto out;
	phy_change = malloc(sizeof(struct _map_phy_change) * num_entries,
	    M_MPR, M_NOWAIT|M_ZERO);
	topo_change.phy_details = phy_change;
	if (!phy_change)
		goto out;
	for (i = 0; i < num_entries; i++, event_phy_change++, phy_change++) {
		phy_change->dev_handle = le16toh(event_phy_change->
		    AttachedDevHandle);
		phy_change->reason = event_phy_change->PhyStatus &
		    MPI2_EVENT_SAS_TOPO_RC_MASK;
	}
	_mapping_update_missing_count(sc, &topo_change);
	_mapping_get_dev_info(sc, &topo_change);
	_mapping_clear_removed_entries(sc);
	_mapping_add_new_device(sc, &topo_change);

out:
	free(topo_change.phy_details, M_MPR);
	_mapping_flush_dpm_pages(sc);
	if (sc->pending_map_events)
		sc->pending_map_events--;
}

/**
 * _mapping_check_update_ir_mt_idx - Check and update IR map table index
 * @sc: per adapter object
 * @event_data: event data payload
 * @evt_idx: current event index
 * @map_idx: current index and the place holder for new map table index
 * @wwid_table: world wide name for volumes in the element table
 *
 * pass through IR events and find whether any events matches and if so
 * tries to find new index if not returns failure
 *
 * Returns 0 on success and 1 on failure
 */
static int
_mapping_check_update_ir_mt_idx(struct mpr_softc *sc,
    Mpi2EventDataIrConfigChangeList_t *event_data, int evt_idx, u32 *map_idx,
    u64 *wwid_table)
{
	struct dev_mapping_table *mt_entry;
	u32 st_idx, end_idx, mt_idx = *map_idx;
	u8 match = 0;
	Mpi2EventIrConfigElement_t *element;
	u16 element_flags;
	int i;

	mt_entry = &sc->mapping_table[mt_idx];
	_mapping_get_ir_maprange(sc, &st_idx, &end_idx);
search_again:
	match = 0;
	for (i = evt_idx + 1; i < event_data->NumElements; i++) {
		element = (Mpi2EventIrConfigElement_t *)
		    &event_data->ConfigElement[i];
		element_flags = le16toh(element->ElementFlags);
		if ((element_flags &
		    MPI2_EVENT_IR_CHANGE_EFLAGS_ELEMENT_TYPE_MASK) !=
		    MPI2_EVENT_IR_CHANGE_EFLAGS_VOLUME_ELEMENT)
			continue;
		if (element->ReasonCode == MPI2_EVENT_IR_CHANGE_RC_ADDED ||
		    element->ReasonCode ==
		    MPI2_EVENT_IR_CHANGE_RC_VOLUME_CREATED) {
			if (mt_entry->physical_id == wwid_table[i]) {
				match = 1;
				break;
			}
		}
	}

	if (match) {
		do {
			mt_idx++;
			if (mt_idx > end_idx)
				return 1;
			mt_entry = &sc->mapping_table[mt_idx];
		} while (mt_entry->device_info & MPR_MAP_IN_USE);
		goto search_again;
	}
	*map_idx = mt_idx;
	return 0;
}

/**
 * mpr_mapping_ir_config_change_event - handle IR config change list events
 * @sc: per adapter object
 * @event_data: event data payload
 *
 * Returns nothing.
 */
void
mpr_mapping_ir_config_change_event(struct mpr_softc *sc,
    Mpi2EventDataIrConfigChangeList_t *event_data)
{
	Mpi2EventIrConfigElement_t *element;
	int i;
	u64 *wwid_table;
	u32 map_idx, flags;
	struct dev_mapping_table *mt_entry;
	u16 element_flags;
	u8 log_full_error = 0;

	wwid_table = malloc(sizeof(u64) * event_data->NumElements, M_MPR,
	    M_NOWAIT | M_ZERO);
	if (!wwid_table)
		goto out;
	element = (Mpi2EventIrConfigElement_t *)&event_data->ConfigElement[0];
	flags = le32toh(event_data->Flags);
	for (i = 0; i < event_data->NumElements; i++, element++) {
		element_flags = le16toh(element->ElementFlags);
		if ((element->ReasonCode != MPI2_EVENT_IR_CHANGE_RC_ADDED) &&
		    (element->ReasonCode != MPI2_EVENT_IR_CHANGE_RC_REMOVED) &&
		    (element->ReasonCode != MPI2_EVENT_IR_CHANGE_RC_NO_CHANGE)
		    && (element->ReasonCode !=
			MPI2_EVENT_IR_CHANGE_RC_VOLUME_CREATED))
			continue;
		if ((element_flags &
		    MPI2_EVENT_IR_CHANGE_EFLAGS_ELEMENT_TYPE_MASK) ==
		    MPI2_EVENT_IR_CHANGE_EFLAGS_VOLUME_ELEMENT) {
			mpr_config_get_volume_wwid(sc,
			    le16toh(element->VolDevHandle), &wwid_table[i]);
			map_idx = _mapping_get_ir_mt_idx_from_wwid(sc,
			    wwid_table[i]);
			if (map_idx != MPR_MAPTABLE_BAD_IDX) {
				mt_entry = &sc->mapping_table[map_idx];
				mt_entry->device_info |= MPR_MAP_IN_USE;
			}
		}
	}
	if (flags == MPI2_EVENT_IR_CHANGE_FLAGS_FOREIGN_CONFIG)
		goto out;
	else {
		element = (Mpi2EventIrConfigElement_t *)&event_data->
		    ConfigElement[0];
		for (i = 0; i < event_data->NumElements; i++, element++) {
			if (element->ReasonCode ==
			    MPI2_EVENT_IR_CHANGE_RC_ADDED ||
			    element->ReasonCode ==
			    MPI2_EVENT_IR_CHANGE_RC_VOLUME_CREATED) {
				map_idx = _mapping_get_ir_mt_idx_from_wwid
				    (sc, wwid_table[i]);
				if (map_idx != MPR_MAPTABLE_BAD_IDX) {
					mt_entry = &sc->mapping_table[map_idx];
					mt_entry->channel = MPR_RAID_CHANNEL;
					mt_entry->id = map_idx;
					mt_entry->dev_handle = le16toh
					    (element->VolDevHandle);
					mt_entry->device_info =
					    MPR_DEV_RESERVED | MPR_MAP_IN_USE;
					_mapping_update_ir_missing_cnt(sc,
					    map_idx, element, wwid_table[i]);
					continue;
				}
				map_idx = _mapping_get_free_ir_mt_idx(sc);
				if (map_idx == MPR_MAPTABLE_BAD_IDX)
					log_full_error = 1;
				else if (i < (event_data->NumElements - 1)) {
					log_full_error =
					    _mapping_check_update_ir_mt_idx
					    (sc, event_data, i, &map_idx,
					     wwid_table);
				}
				if (log_full_error) {
					printf("%s: no space to add the RAID "
					    "volume with handle 0x%04x in "
					    "mapping table\n", __func__, le16toh
					    (element->VolDevHandle));
					continue;
				}
				mt_entry = &sc->mapping_table[map_idx];
				mt_entry->physical_id = wwid_table[i];
				mt_entry->channel = MPR_RAID_CHANNEL;
				mt_entry->id = map_idx;
				mt_entry->dev_handle = le16toh(element->
				    VolDevHandle);
				mt_entry->device_info = MPR_DEV_RESERVED |
				    MPR_MAP_IN_USE;
				mt_entry->init_complete = 0;
				_mapping_update_ir_missing_cnt(sc, map_idx,
				    element, wwid_table[i]);
			} else if (element->ReasonCode ==
			    MPI2_EVENT_IR_CHANGE_RC_REMOVED) {
				map_idx = _mapping_get_ir_mt_idx_from_wwid(sc,
				    wwid_table[i]);
				if (map_idx == MPR_MAPTABLE_BAD_IDX) {
					printf("%s: failed to remove a volume "
					    "because it has already been "
					    "removed\n", __func__);
					continue;
				}
				_mapping_update_ir_missing_cnt(sc, map_idx,
				    element, wwid_table[i]);
			} else if (element->ReasonCode ==
			    MPI2_EVENT_IR_CHANGE_RC_VOLUME_DELETED) {
				map_idx = _mapping_get_mt_idx_from_handle(sc,
				    le16toh(element->VolDevHandle));
				if (map_idx == MPR_MAPTABLE_BAD_IDX) {
					printf("%s: failed to remove volume "
					    "with handle 0x%04x because it has "
					    "already been removed\n", __func__,
					    le16toh(element->VolDevHandle));
					continue;
				}
				mt_entry = &sc->mapping_table[map_idx];
				_mapping_update_ir_missing_cnt(sc, map_idx,
				    element, mt_entry->physical_id);
			}
		}
	}

out:
	_mapping_flush_dpm_pages(sc);
	free(wwid_table, M_MPR);
	if (sc->pending_map_events)
		sc->pending_map_events--;
}
