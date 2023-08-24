/*-
 * Copyright 2016-2023 Microchip Technology, Inc. and/or its subsidiaries.
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


#include "smartpqi_includes.h"

/*
 * Checks a firmware feature status, given bit position.
 */
static inline boolean_t
pqi_is_firmware_feature_supported(
	struct pqi_config_table_firmware_features *firmware_features,
	unsigned int bit_position)
{
	unsigned int byte_index;

	byte_index = bit_position / BITS_PER_BYTE;

	if (byte_index >= firmware_features->num_elements) {
		DBG_ERR_NO_SOFTS("Invalid byte index for bit position %u\n",
			bit_position);
		return false;
	}

	return (firmware_features->features_supported[byte_index] &
		(1 << (bit_position % BITS_PER_BYTE))) ? true : false;
}

/*
 * Counts down into the enabled section of firmware
 * features and reports current enabled status, given
 * bit position.
 */
static inline boolean_t
pqi_is_firmware_feature_enabled(
	struct pqi_config_table_firmware_features *firmware_features,
	uint8_t *firmware_features_iomem_addr,
	unsigned int bit_position)
{
	unsigned int byte_index;
	uint8_t *features_enabled_iomem_addr;

	byte_index = (bit_position / BITS_PER_BYTE) +
		(firmware_features->num_elements * 2);

	features_enabled_iomem_addr = firmware_features_iomem_addr +
		offsetof(struct pqi_config_table_firmware_features,
			features_supported) + byte_index;

	return (*features_enabled_iomem_addr &
		(1 << (bit_position % BITS_PER_BYTE))) ? true : false;
}

/*
 * Sets the given bit position for the driver to request the indicated
 * firmware feature be enabled.
 */
static inline void
pqi_request_firmware_feature(
	struct pqi_config_table_firmware_features *firmware_features,
	unsigned int bit_position)
{
	unsigned int byte_index;

	/* byte_index adjusted to index into requested start bits */
	byte_index = (bit_position / BITS_PER_BYTE) +
		firmware_features->num_elements;

	/* setting requested bits of local firmware_features */
	firmware_features->features_supported[byte_index] |=
		(1 << (bit_position % BITS_PER_BYTE));
}

/*
 * Creates and sends the request for firmware to update the config
 * table.
 */
static int
pqi_config_table_update(pqisrc_softstate_t *softs,
	uint16_t first_section, uint16_t last_section)
{
	struct pqi_vendor_general_request request;
	int ret;

	memset(&request, 0, sizeof(request));

	request.header.iu_type = PQI_REQUEST_IU_VENDOR_GENERAL;
	request.header.iu_length = sizeof(request) - PQI_REQUEST_HEADER_LENGTH;
	request.function_code = PQI_VENDOR_GENERAL_CONFIG_TABLE_UPDATE;
	request.data.config_table_update.first_section = first_section;
	request.data.config_table_update.last_section = last_section;

	ret = pqisrc_build_send_vendor_request(softs, &request);

	if (ret != PQI_STATUS_SUCCESS) {
		DBG_ERR("Failed to submit vendor general request IU, Ret status: %d\n", ret);
	}

	return ret;
}

/*
 * Copies requested features bits into firmware config table,
 * checks for support, and returns status of updating the config table.
 */
static int
pqi_enable_firmware_features(pqisrc_softstate_t *softs,
	struct pqi_config_table_firmware_features *firmware_features,
	uint8_t *firmware_features_abs_addr)
{
	uint8_t *features_requested;
	uint8_t *features_requested_abs_addr;
	uint16_t *host_max_known_feature_iomem_addr;
	uint16_t pqi_max_feature = PQI_FIRMWARE_FEATURE_MAXIMUM;

	features_requested = firmware_features->features_supported +
		firmware_features->num_elements;

	features_requested_abs_addr = firmware_features_abs_addr +
		(features_requested - (uint8_t*)firmware_features);
	/*
	 * NOTE: This memcpy is writing to a BAR-mapped address
	 * which may not be safe for all OSes without proper API
	 */
	memcpy(features_requested_abs_addr, features_requested,
		firmware_features->num_elements);

	if (pqi_is_firmware_feature_supported(firmware_features,
		PQI_FIRMWARE_FEATURE_MAX_KNOWN_FEATURE)) {
		host_max_known_feature_iomem_addr =
			(uint16_t*)(features_requested_abs_addr +
			(firmware_features->num_elements * 2) + sizeof(uint16_t));
			/*
			 * NOTE: This writes to a BAR-mapped address
			 * which may not be safe for all OSes without proper API
			 */
			*host_max_known_feature_iomem_addr = pqi_max_feature;
	}

	return pqi_config_table_update(softs,
		PQI_CONF_TABLE_SECTION_FIRMWARE_FEATURES,
		PQI_CONF_TABLE_SECTION_FIRMWARE_FEATURES);
}

typedef struct pqi_firmware_feature pqi_firmware_feature_t;
typedef void (*feature_status_fn)(pqisrc_softstate_t *softs,
	pqi_firmware_feature_t *firmware_feature);

struct pqi_firmware_feature {
	char		*feature_name;
	unsigned int	feature_bit;
	boolean_t		supported;
	boolean_t		enabled;
	feature_status_fn	feature_status;
};

static void
pqi_firmware_feature_status(pqisrc_softstate_t *softs,
	struct pqi_firmware_feature *firmware_feature)
{
	if (!firmware_feature->supported) {
		DBG_NOTE("%s not supported by controller\n",
			firmware_feature->feature_name);
		return;
	}

	if (firmware_feature->enabled) {
		DBG_NOTE("%s enabled\n", firmware_feature->feature_name);
		return;
	}

	DBG_NOTE("failed to enable %s\n", firmware_feature->feature_name);
}

static void
pqi_ctrl_update_feature_flags(pqisrc_softstate_t *softs,
	struct pqi_firmware_feature *firmware_feature)
{
	switch (firmware_feature->feature_bit) {
	case PQI_FIRMWARE_FEATURE_RAID_1_WRITE_BYPASS:
		softs->aio_raid1_write_bypass = firmware_feature->enabled;
		break;
	case PQI_FIRMWARE_FEATURE_RAID_5_WRITE_BYPASS:
		softs->aio_raid5_write_bypass = firmware_feature->enabled;
		break;
	case PQI_FIRMWARE_FEATURE_RAID_6_WRITE_BYPASS:
		softs->aio_raid6_write_bypass = firmware_feature->enabled;
		break;
	case PQI_FIRMWARE_FEATURE_RAID_IU_TIMEOUT:
		softs->timeout_in_passthrough = true;
		break;
	case PQI_FIRMWARE_FEATURE_TMF_IU_TIMEOUT:
		softs->timeout_in_tmf = true;
		break;
	case PQI_FIRMWARE_FEATURE_UNIQUE_SATA_WWN:
		break;
	case PQI_FIRMWARE_FEATURE_PAGE83_IDENTIFIER_FOR_RPL_WWID:
		softs->page83id_in_rpl = true;
		break;
	default:
		DBG_NOTE("Nothing to do\n");
		return;
		break;
	}
	/* for any valid feature, also go update the feature status. */
	pqi_firmware_feature_status(softs, firmware_feature);
}


static inline void
pqi_firmware_feature_update(pqisrc_softstate_t *softs,
	struct pqi_firmware_feature *firmware_feature)
{
	if (firmware_feature->feature_status)
		firmware_feature->feature_status(softs, firmware_feature);
}

/* Defines PQI features that driver wishes to support */
static struct pqi_firmware_feature pqi_firmware_features[] = {
#if 0
	{
		.feature_name = "Online Firmware Activation",
		.feature_bit = PQI_FIRMWARE_FEATURE_OFA,
		.feature_status = pqi_firmware_feature_status,
	},
	{
		.feature_name = "Serial Management Protocol",
		.feature_bit = PQI_FIRMWARE_FEATURE_SMP,
		.feature_status = pqi_firmware_feature_status,
	},
#endif
	{
		.feature_name = "SATA WWN Unique ID",
		.feature_bit = PQI_FIRMWARE_FEATURE_UNIQUE_SATA_WWN,
		.feature_status = pqi_ctrl_update_feature_flags,
	},
	{
		.feature_name = "RAID IU Timeout",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_IU_TIMEOUT,
		.feature_status = pqi_ctrl_update_feature_flags,
	},
	{
		.feature_name = "TMF IU Timeout",
		.feature_bit = PQI_FIRMWARE_FEATURE_TMF_IU_TIMEOUT,
		.feature_status = pqi_ctrl_update_feature_flags,
	},
	{
		.feature_name = "Support for RPL WWID filled by Page83 identifier",
		.feature_bit = PQI_FIRMWARE_FEATURE_PAGE83_IDENTIFIER_FOR_RPL_WWID,
		.feature_status = pqi_ctrl_update_feature_flags,
	},
	/* Features independent of Maximum Known Feature should be added
	before Maximum Known Feature*/
	{
		.feature_name = "Maximum Known Feature",
		.feature_bit = PQI_FIRMWARE_FEATURE_MAX_KNOWN_FEATURE,
		.feature_status = pqi_firmware_feature_status,
	},
	{
		.feature_name = "RAID 0 Read Bypass",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_0_READ_BYPASS,
		.feature_status = pqi_firmware_feature_status,
	},
	{
		.feature_name = "RAID 1 Read Bypass",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_1_READ_BYPASS,
		.feature_status = pqi_firmware_feature_status,
	},
	{
		.feature_name = "RAID 5 Read Bypass",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_5_READ_BYPASS,
		.feature_status = pqi_firmware_feature_status,
	},
	{
		.feature_name = "RAID 6 Read Bypass",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_6_READ_BYPASS,
		.feature_status = pqi_firmware_feature_status,
	},
	{
		.feature_name = "RAID 0 Write Bypass",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_0_WRITE_BYPASS,
		.feature_status = pqi_firmware_feature_status,
	},
	{
		.feature_name = "RAID 1 Write Bypass",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_1_WRITE_BYPASS,
		.feature_status = pqi_ctrl_update_feature_flags,
	},
	{
		.feature_name = "RAID 5 Write Bypass",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_5_WRITE_BYPASS,
		.feature_status = pqi_ctrl_update_feature_flags,
	},
	{
		.feature_name = "RAID 6 Write Bypass",
		.feature_bit = PQI_FIRMWARE_FEATURE_RAID_6_WRITE_BYPASS,
		.feature_status = pqi_ctrl_update_feature_flags,
	},
#if 0
	{
		.feature_name = "New Soft Reset Handshake",
		.feature_bit = PQI_FIRMWARE_FEATURE_SOFT_RESET_HANDSHAKE,
		.feature_status = pqi_ctrl_update_feature_flags,
	},
#endif

};

static void
pqi_process_firmware_features(pqisrc_softstate_t *softs,
	void *features, void *firmware_features_abs_addr)
{
	int rc;
	struct pqi_config_table_firmware_features *firmware_features = features;
	unsigned int i;
	unsigned int num_features_supported;

	/* Iterates through local PQI feature support list to
	see if the controller also supports the feature */
	for (i = 0, num_features_supported = 0;
		i < ARRAY_SIZE(pqi_firmware_features); i++) {
		/*Check if SATA_WWN_FOR_DEV_UNIQUE_ID feature enabled by setting module
		parameter if not avoid checking for the feature*/
		if ((pqi_firmware_features[i].feature_bit ==
			PQI_FIRMWARE_FEATURE_UNIQUE_SATA_WWN) &&
			(!softs->sata_unique_wwn)) {
			continue;
		}
		if (pqi_is_firmware_feature_supported(firmware_features,
			pqi_firmware_features[i].feature_bit)) {
			pqi_firmware_features[i].supported = true;
			num_features_supported++;
		} else {
			DBG_WARN("Feature %s is not supported by firmware\n",
			pqi_firmware_features[i].feature_name);
			pqi_firmware_feature_update(softs,
				&pqi_firmware_features[i]);

			/* if max known feature bit isn't supported,
 			 * then no other feature bits are supported.
 			 */
			if (pqi_firmware_features[i].feature_bit ==
				PQI_FIRMWARE_FEATURE_MAX_KNOWN_FEATURE)
				break;
		}
	}

	DBG_INFO("Num joint features supported : %u \n", num_features_supported);

	if (num_features_supported == 0)
		return;

	/* request driver features that are also on firmware-supported list */
	for (i = 0; i < ARRAY_SIZE(pqi_firmware_features); i++) {
		if (!pqi_firmware_features[i].supported)
			continue;
#ifdef DEVICE_HINT
		if (check_device_hint_status(softs, pqi_firmware_features[i].feature_bit))
			continue;
#endif
		pqi_request_firmware_feature(firmware_features,
			pqi_firmware_features[i].feature_bit);
	}

	/* enable the features that were successfully requested. */
	rc = pqi_enable_firmware_features(softs, firmware_features,
		firmware_features_abs_addr);
	if (rc) {
		DBG_ERR("failed to enable firmware features in PQI configuration table\n");
		for (i = 0; i < ARRAY_SIZE(pqi_firmware_features); i++) {
			if (!pqi_firmware_features[i].supported)
				continue;
			pqi_firmware_feature_update(softs,
				&pqi_firmware_features[i]);
		}
		return;
	}

	/* report the features that were successfully enabled. */
	for (i = 0; i < ARRAY_SIZE(pqi_firmware_features); i++) {
		if (!pqi_firmware_features[i].supported)
			continue;
		if (pqi_is_firmware_feature_enabled(firmware_features,
			firmware_features_abs_addr,
			pqi_firmware_features[i].feature_bit)) {
				pqi_firmware_features[i].enabled = true;
		} else {
			DBG_WARN("Feature %s could not be enabled.\n",
				pqi_firmware_features[i].feature_name);
		}
		pqi_firmware_feature_update(softs,
			&pqi_firmware_features[i]);
	}
}

static void
pqi_init_firmware_features(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pqi_firmware_features); i++) {
		pqi_firmware_features[i].supported = false;
		pqi_firmware_features[i].enabled = false;
	}
}

static void
pqi_process_firmware_features_section(pqisrc_softstate_t *softs,
	void *features, void *firmware_features_abs_addr)
{
	pqi_init_firmware_features();
	pqi_process_firmware_features(softs, features, firmware_features_abs_addr);
}


/*
 * Get the PQI configuration table parameters.
 * Currently using for heart-beat counter scratch-pad register.
 */
int
pqisrc_process_config_table(pqisrc_softstate_t *softs)
{
	int ret = PQI_STATUS_FAILURE;
	uint32_t config_table_size;
	uint32_t section_off;
	uint8_t *config_table_abs_addr;
	struct pqi_conf_table *conf_table;
	struct pqi_conf_table_section_header *section_hdr;

	config_table_size = softs->pqi_cap.conf_tab_sz;

	if (config_table_size < sizeof(*conf_table) ||
		config_table_size > PQI_CONF_TABLE_MAX_LEN) {
		DBG_ERR("Invalid PQI conf table length of %u\n",
			config_table_size);
		return ret;
	}

	conf_table = os_mem_alloc(softs, config_table_size);
	if (!conf_table) {
		DBG_ERR("Failed to allocate memory for PQI conf table\n");
		return ret;
	}

	config_table_abs_addr = (uint8_t *)(softs->pci_mem_base_vaddr +
					softs->pqi_cap.conf_tab_off);

	PCI_MEM_GET_BUF(softs, config_table_abs_addr,
			softs->pqi_cap.conf_tab_off,
			(uint8_t*)conf_table, config_table_size);

	if (memcmp(conf_table->sign, PQI_CONF_TABLE_SIGNATURE,
			sizeof(conf_table->sign)) != 0) {
		DBG_ERR("Invalid PQI config signature\n");
		goto out;
	}

	section_off = LE_32(conf_table->first_section_off);

	while (section_off) {

		if (section_off+ sizeof(*section_hdr) >= config_table_size) {
			DBG_INFO("Reached end of PQI config table. Breaking off.\n");
			break;
		}

		section_hdr = (struct pqi_conf_table_section_header *)((uint8_t *)conf_table + section_off);

		switch (LE_16(section_hdr->section_id)) {
		case PQI_CONF_TABLE_SECTION_GENERAL_INFO:
			break;
		case PQI_CONF_TABLE_SECTION_FIRMWARE_FEATURES:
			pqi_process_firmware_features_section(softs, section_hdr, (config_table_abs_addr + section_off));
			break;
		case PQI_CONF_TABLE_SECTION_FIRMWARE_ERRATA:
		case PQI_CONF_TABLE_SECTION_DEBUG:
			break;
		case PQI_CONF_TABLE_SECTION_HEARTBEAT:
			softs->heartbeat_counter_off = softs->pqi_cap.conf_tab_off +
				section_off +
				offsetof(struct pqi_conf_table_heartbeat, heartbeat_counter);
			softs->heartbeat_counter_abs_addr = (uint64_t *)(softs->pci_mem_base_vaddr +
				softs->heartbeat_counter_off);
			ret = PQI_STATUS_SUCCESS;
			break;
		case PQI_CONF_TABLE_SOFT_RESET:
			break;
		default:
			DBG_NOTE("unrecognized PQI config table section ID: 0x%x\n",
				LE_16(section_hdr->section_id));
			break;
		}
		section_off = LE_16(section_hdr->next_section_off);
	}
out:
	os_mem_free(softs, (void *)conf_table,config_table_size);
	return ret;
}
