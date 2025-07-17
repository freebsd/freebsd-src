/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_CFG_BUNDLE_H_
#define ADF_CFG_BUNDLE_H_

#include "adf_accel_devices.h"
#include "adf_cfg_common.h"

#define MAX_SECTIONS_PER_BUNDLE 8
#define MAX_SECTION_NAME_LEN 64

#define TX 0x0
#define RX 0x1

#define ASSIGN_SERV_TO_RINGS(bund, index, base, stype, rng_per_srv)            \
	do {                                                                   \
		int j = 0;                                                     \
		typeof(bund) b = (bund);                                       \
		typeof(index) i = (index);                                     \
		typeof(base) s = (base);                                       \
		typeof(stype) t = (stype);                                     \
		typeof(rng_per_srv) rps = (rng_per_srv);                       \
		for (j = 0; j < rps; j++) {                                    \
			b->rings[i + j]->serv_type = t;                        \
			b->rings[i + j + s]->serv_type = t;                    \
		}                                                              \
	} while (0)

bool adf_cfg_is_free(struct adf_cfg_bundle *bundle);

int adf_cfg_get_ring_pairs_from_bundle(struct adf_cfg_bundle *bundle,
				       struct adf_cfg_instance *inst,
				       const char *process_name,
				       struct adf_cfg_instance *bundle_inst);

struct adf_cfg_instance *
adf_cfg_get_free_instance(struct adf_cfg_device *device,
			  struct adf_cfg_bundle *bundle,
			  struct adf_cfg_instance *inst,
			  const char *process_name);

int adf_cfg_bundle_init(struct adf_cfg_bundle *bundle,
			struct adf_cfg_device *device,
			int bank_num,
			struct adf_accel_dev *accel_dev);

void adf_cfg_bundle_clear(struct adf_cfg_bundle *bundle,
			  struct adf_accel_dev *accel_dev);

void adf_cfg_init_ring2serv_mapping(struct adf_accel_dev *accel_dev,
				    struct adf_cfg_bundle *bundle,
				    struct adf_cfg_device *device);

int adf_cfg_rel_ring2serv_mapping(struct adf_cfg_bundle *bundle);

static inline void
adf_get_ring_svc_map_data(struct adf_hw_device_data *hw_data,
			  int bundle_num,
			  int ring_pair_index,
			  u8 *serv_type,
			  int *ring_index,
			  int *num_rings_per_srv)
{
	if (hw_data->get_ring_svc_map_data)
		return hw_data->get_ring_svc_map_data(ring_pair_index,
						      hw_data->ring_to_svc_map,
						      serv_type,
						      ring_index,
						      num_rings_per_srv,
						      bundle_num);
	*serv_type = GET_SRV_TYPE(hw_data->ring_to_svc_map, ring_pair_index);
	*num_rings_per_srv =
	    hw_data->num_rings_per_bank / (2 * ADF_CFG_NUM_SERVICES);
	*ring_index = (*num_rings_per_srv) * ring_pair_index;
}
#endif
