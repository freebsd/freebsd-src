/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "adf_cfg_instance.h"

void
crypto_instance_init(struct adf_cfg_instance *instance,
		     struct adf_cfg_bundle *bundle)
{
	int i = 0;

	instance->stype = CRYPTO;
	for (i = 0; i < bundle->num_of_rings / 2; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_ASYM &&
		    bundle->rings[i]->mode == TX) {
			instance->asym_tx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}

	for (i = 0; i < bundle->num_of_rings / 2; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_SYM &&
		    bundle->rings[i]->mode == TX) {
			instance->sym_tx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}

	for (i = bundle->num_of_rings / 2; i < bundle->num_of_rings; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_ASYM &&
		    bundle->rings[i]->mode == RX) {
			instance->asym_rx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}

	for (i = bundle->num_of_rings / 2; i < bundle->num_of_rings; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_SYM &&
		    bundle->rings[i]->mode == RX) {
			instance->sym_rx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}
}

void
dc_instance_init(struct adf_cfg_instance *instance,
		 struct adf_cfg_bundle *bundle)
{
	int i = 0;

	instance->stype = COMP;
	for (i = 0; i < bundle->num_of_rings / 2; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_DC &&
		    bundle->rings[i]->mode == TX) {
			instance->dc_tx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}

	for (i = bundle->num_of_rings / 2; i < bundle->num_of_rings; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_DC &&
		    bundle->rings[i]->mode == RX) {
			instance->dc_rx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}
}

void
asym_instance_init(struct adf_cfg_instance *instance,
		   struct adf_cfg_bundle *bundle)
{
	int i = 0;

	instance->stype = ASYM;
	for (i = 0; i < bundle->num_of_rings / 2; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_ASYM &&
		    bundle->rings[i]->mode == TX) {
			instance->asym_tx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}

	for (i = bundle->num_of_rings / 2; i < bundle->num_of_rings; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_ASYM &&
		    bundle->rings[i]->mode == RX) {
			instance->asym_rx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}
}

void
sym_instance_init(struct adf_cfg_instance *instance,
		  struct adf_cfg_bundle *bundle)
{
	int i = 0;

	instance->stype = SYM;
	for (i = 0; i < bundle->num_of_rings / 2; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_SYM &&
		    bundle->rings[i]->mode == TX) {
			instance->sym_tx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}

	for (i = 0 + bundle->num_of_rings / 2; i < bundle->num_of_rings; i++) {
		if ((bundle->in_use >> bundle->rings[i]->number) & 0x1)
			continue;

		if (bundle->rings[i]->serv_type == ADF_ACCEL_SERV_SYM &&
		    bundle->rings[i]->mode == RX) {
			instance->sym_rx = bundle->rings[i]->number;
			bundle->in_use |= 1 << bundle->rings[i]->number;
			break;
		}
	}
}
