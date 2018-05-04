/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Khamba Staring <k.staring@quickdecay.com>
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
 * $FreeBSD$
 */

#ifndef PIN_PATCH_REALTEK_H
#define PIN_PATCH_REALTEK_H

#include "hdac.h"
#include "pin_patch.h"

/*
 * Pin patches
 */
static struct pin_patch_t pin_patches_lg_lw20[] = {
	{
		.nid = 26,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {	HDA_CONFIG_DEFAULTCONF_DEVICE_MASK,
					HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN }
	}, {
		.nid = 27,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {	HDA_CONFIG_DEFAULTCONF_DEVICE_MASK,
					HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT }
	}, { }
};

static struct pin_patch_t pin_patches_clevo_d900t_asus_m5200[] = {
	{
		.nid = 24,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {	HDA_CONFIG_DEFAULTCONF_DEVICE_MASK,
					HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN }
	}, {
		.nid = 25,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {	HDA_CONFIG_DEFAULTCONF_DEVICE_MASK,
					HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN }
	}, {
		.nid = 26,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {	HDA_CONFIG_DEFAULTCONF_DEVICE_MASK,
					HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN }
	}, {
		.nid = 27,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {	HDA_CONFIG_DEFAULTCONF_DEVICE_MASK,
					HDA_CONFIG_DEFAULTCONF_DEVICE_LINE_IN }
	}, {
		.nid = 28,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {	HDA_CONFIG_DEFAULTCONF_DEVICE_MASK,
					HDA_CONFIG_DEFAULTCONF_DEVICE_CD }
	}, { }
};

static struct pin_patch_t pin_patches_msi_ms034a[] = {
	{
		.nid = 25,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED }
	}, {
		.nid = 28,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_CD |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED }
	}, { }
};

static struct pin_patch_t pin_patches_asus_w6f[] = {
	{
		.nid = 11,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED }
	}, {
		.nid = 12,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED }
	}, {
		.nid = 14,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED }
	}, {
		.nid = 15,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK }
	}, {
		.nid = 16,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED }
	}, {
		.nid = 31,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED }
	}, {
		.nid = 32,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_MIC_IN |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_FIXED }
	}, { }
};

static struct pin_patch_t pin_patches_uniwill_9075[] = {
	{
		.nid = 15,
		.type = PIN_PATCH_TYPE_MASK,
		.patch.mask = {
			HDA_CONFIG_DEFAULTCONF_DEVICE_MASK |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_MASK,
			HDA_CONFIG_DEFAULTCONF_DEVICE_HP_OUT |
                        HDA_CONFIG_DEFAULTCONF_CONNECTIVITY_JACK }
	}, { }
};

static struct pin_patch_t pin_patches_dell_xps_jack[] = {
	PIN_PATCH_JACK_WO_DETECT(24),
	PIN_PATCH_HPMIC_WO_DETECT(26),
	{ }
};

/*
 * List of models and patches
 */
static struct hdaa_model_pin_patch_t realtek_model_pin_patches[] = {
	{ /**** CODEC: HDA_CODEC_ALC260 ****/
		.id = HDA_CODEC_ALC260,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(SONY_S5_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(16, "seq=15 device=Headphones"),
        				{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC268 ****/
		.id = HDA_CODEC_ALC268,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_T5320_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(20, "as=1 seq=15"),
        				{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC269 ****/
		.id = HDA_CODEC_ALC269,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LENOVO_X1CRBN_SUBVENDOR),
					PIN_SUBVENDOR(LENOVO_T430_SUBVENDOR),
					PIN_SUBVENDOR(LENOVO_T430S_SUBVENDOR),
					PIN_SUBVENDOR(LENOVO_T530_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(21, "as=1 seq=15"),
        				{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_UX31A_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(33, "as=1 seq=15"),
        				{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC288 ****/
		.id = HDA_CODEC_ALC288,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(DELL_E7240_SUBVENDOR),
					{ }
				},
				.pin_patches = pin_patches_dell_xps_jack
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC292 ****/
		.id = HDA_CODEC_ALC292,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LENOVO_X120BS_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(21, "as=1 seq=15"),
        				{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC298 ****/
		.id = HDA_CODEC_ALC298,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(DELL_XPS9560_SUBVENDOR),
					{ }
				},
				.pin_patches = pin_patches_dell_xps_jack
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC861 ****/
		.id = HDA_CODEC_ALC861,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_W6F_SUBVENDOR),
					{ }
				},
				.pin_patches = pin_patches_asus_w6f
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(UNIWILL_9075_SUBVENDOR),
					{ }
				},
				.pin_patches = pin_patches_uniwill_9075
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC880 ****/
		.id = HDA_CODEC_ALC880,
		.patches = (struct model_pin_patch_t[]){
			{ // old patch
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LG_LW20_SUBVENDOR),
					{ }
				},
				.pin_patches = pin_patches_lg_lw20
			}, { // old patch
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(CLEVO_D900T_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_M5200_SUBVENDOR),
					{ }
				},
				.pin_patches = pin_patches_clevo_d900t_asus_m5200
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LG_M1_SUBVENDOR),
					PIN_SUBVENDOR(LG_P1_SUBVENDOR),
					PIN_SUBVENDOR(LG_W1_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_NOT_APPLICABLE(0x16),
					PIN_PATCH_NOT_APPLICABLE(0x18),
					PIN_PATCH_NOT_APPLICABLE(0x1a),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(COEUS_G610P_SUBVENDOR),
					PIN_SUBVENDOR(ARIMA_W810_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_NOT_APPLICABLE(0x17),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(FS_AMILO_M1437_SUBVENDOR),
					PIN_SUBVENDOR(FS_AMILO_M1451G_SUBVENDOR),
					PIN_SUBVENDOR(FS_AMILO_PI1556_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HP_OUT(0x14),
					PIN_PATCH_SPEAKER(0x15),
					PIN_PATCH_BASS_SPEAKER(0x16),
					PIN_PATCH_NOT_APPLICABLE(0x17),
					PIN_PATCH_NOT_APPLICABLE(0x18),
					PIN_PATCH_MIC_IN(0x19),
					PIN_PATCH_NOT_APPLICABLE(0x1a),
					PIN_PATCH_NOT_APPLICABLE(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_PATCH_NOT_APPLICABLE(0x1e),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(UNIWILL_9054_SUBVENDOR),
					PIN_SUBVENDOR(FS_AMILO_XI1526_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HP_OUT(0x14),
					PIN_PATCH_SPEAKER(0x15),
					PIN_PATCH_NOT_APPLICABLE(0x16),
					PIN_PATCH_NOT_APPLICABLE(0x17),
					PIN_PATCH_NOT_APPLICABLE(0x18),
					PIN_PATCH_MIC_IN(0x19),
					PIN_PATCH_NOT_APPLICABLE(0x1a),
					PIN_PATCH_NOT_APPLICABLE(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_PATCH_SPDIF_OUT(0x1e),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LG_LW25_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_OVERRIDE(0x1a, 0x0181344f), /* li */
					PIN_OVERRIDE(0x1b, 0x0321403f), /* hp */
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(UNIWILL_9070_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HP(0x14),
					PIN_PATCH_SPEAKER(0x15),
					PIN_PATCH_BASS_SPEAKER(0x16),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(UNIWILL_9050_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_NOT_APPLICABLE(0x17),
					PIN_PATCH_NOT_APPLICABLE(0x19),
					PIN_PATCH_NOT_APPLICABLE(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1f),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_Z71V_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_SPEAKER(0x14),
					PIN_PATCH_HP(0x15),
					PIN_PATCH_NOT_APPLICABLE(0x16),
					PIN_PATCH_NOT_APPLICABLE(0x17),
					PIN_PATCH_MIC_IN(0x18),
					PIN_PATCH_NOT_APPLICABLE(0x19),
					PIN_PATCH_LINE_IN(0x1a),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_W5A_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HP(0x14),
					PIN_PATCH_NOT_APPLICABLE(0x15),
					PIN_PATCH_NOT_APPLICABLE(0x16),
					PIN_PATCH_NOT_APPLICABLE(0x17),
					PIN_PATCH_MIC_INTERNAL(0x18),
					PIN_PATCH_NOT_APPLICABLE(0x19),
					PIN_PATCH_NOT_APPLICABLE(0x1a),
					PIN_PATCH_NOT_APPLICABLE(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_OVERRIDE(0x1e, 0xb743111e), /* spdif out */
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_E310ID_SUBVENDOR),
					PIN_SUBVENDOR(SONY_81A0ID_SUBVENDOR),
					PIN_SUBVENDOR(SONY_81D6ID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_LINE_OUT(0x14),
					PIN_PATCH_NOT_APPLICABLE(0x15),
					PIN_PATCH_NOT_APPLICABLE(0x16),
					PIN_PATCH_NOT_APPLICABLE(0x17),
					PIN_OVERRIDE(0x18, 0x01a19c30), /* mic in */
					PIN_PATCH_HP(0x19),
					PIN_PATCH_LINE_IN(0x1a),
					PIN_PATCH_MIC_FRONT(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_PATCH_NOT_APPLICABLE(0x1e),
					PIN_PATCH_NOT_APPLICABLE(0x1f),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_0070ID_SUBVENDOR),
					PIN_SUBVENDOR(ACER_E309ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_D402ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_E305ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_E308ID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_LINE_OUT(0x14),
					PIN_PATCH_NOT_APPLICABLE(0x15),
					PIN_PATCH_NOT_APPLICABLE(0x16),
					PIN_PATCH_NOT_APPLICABLE(0x17),
					PIN_OVERRIDE(0x18, 0x01a19c30), /* mic in */
					PIN_PATCH_HP(0x19),
					PIN_PATCH_LINE_IN(0x1a),
					PIN_PATCH_MIC_FRONT(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_OVERRIDE(0x1e, 0x0144111e), /* spdif */
					PIN_PATCH_NOT_APPLICABLE(0x1f),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(GATEWAY_3032ID_SUBVENDOR),
					PIN_SUBVENDOR(GATEWAY_3033ID_SUBVENDOR),
					PIN_SUBVENDOR(GATEWAY_4039ID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_LINE_OUT(0x14),
					PIN_PATCH_NOT_APPLICABLE(0x15),
					PIN_PATCH_CLFE(0x16),
					PIN_PATCH_SURROUND(0x17),
					PIN_OVERRIDE(0x18, 0x01a19c30), /* mic in */
					PIN_PATCH_HP(0x19),
					PIN_PATCH_LINE_IN(0x1a),
					PIN_PATCH_MIC_FRONT(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_PATCH_NOT_APPLICABLE(0x1e),
					PIN_PATCH_NOT_APPLICABLE(0x1f),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(COEUS_A880ID_SUBVENDOR),
					PIN_SUBVENDOR(BIOSTAR_8202ID_SUBVENDOR),
					PIN_SUBVENDOR(EPOX_400DID_SUBVENDOR),
					PIN_SUBVENDOR(EPOX_EP5LDA_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_A100ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_D400ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_D401ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_E224ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_E400ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_E401ID_SUBVENDOR),
					PIN_SUBVENDOR(INTEL_E402ID_SUBVENDOR),
					PIN_SUBVENDOR(AOPEN_I915GMMHFS_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_LINE_OUT(0x14),
					PIN_PATCH_NOT_APPLICABLE(0x15),
					PIN_PATCH_CLFE(0x16),
					PIN_PATCH_SURROUND(0x17),
					PIN_OVERRIDE(0x18, 0x01a19c30), /* mic in */
					PIN_PATCH_HP(0x19),
					PIN_PATCH_LINE_IN(0x1a),
					PIN_PATCH_MIC_FRONT(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_OVERRIDE(0x1e, 0x0144111e), /* spdif */
					PIN_PATCH_NOT_APPLICABLE(0x1f),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_APFV_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_LINE_OUT(0x14),
					PIN_PATCH_SURROUND(0x15),
					PIN_PATCH_CLFE(0x16),
					PIN_OVERRIDE(0x17, 0x01012414), /* side */
					PIN_OVERRIDE(0x18, 0x01a19c30), /* mic in */
					PIN_PATCH_MIC_FRONT(0x19),
					PIN_PATCH_LINE_IN(0x1a),
					PIN_PATCH_HP(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_PATCH_NOT_APPLICABLE(0x1e),
					PIN_PATCH_NOT_APPLICABLE(0x1f),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_0077ID_SUBVENDOR),
					PIN_SUBVENDOR(ACER_0078ID_SUBVENDOR),
					PIN_SUBVENDOR(ACER_0087ID_SUBVENDOR),
					PIN_SUBVENDOR(SHUTTLE_ST20G5_SUBVENDOR),
					PIN_SUBVENDOR(GB_K8_SUBVENDOR),
					PIN_SUBVENDOR(MSI_1150ID_SUBVENDOR),
					PIN_SUBVENDOR(FIC_P4M_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_LINE_OUT(0x14),
					PIN_PATCH_SURROUND(0x15),
					PIN_PATCH_CLFE(0x16),
					PIN_OVERRIDE(0x17, 0x01012414), /* side */
					PIN_OVERRIDE(0x18, 0x01a19c30), /* mic in */
					PIN_PATCH_MIC_FRONT(0x19),
					PIN_PATCH_LINE_IN(0x1a),
					PIN_PATCH_HP(0x1b),
					PIN_PATCH_NOT_APPLICABLE(0x1c),
					PIN_PATCH_NOT_APPLICABLE(0x1d),
					PIN_OVERRIDE(0x1e, 0x0144111e), /* spdif */
					PIN_PATCH_NOT_APPLICABLE(0x1f),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC883 ****/
		.id = HDA_CODEC_ALC883,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(MSI_MS034A_SUBVENDOR),
					{ }
				},
				.pin_patches = pin_patches_msi_ms034a
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC892 ****/
		.id = HDA_CODEC_ALC892,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(INTEL_DH87RL_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(27, "as=1 seq=15"),
        				{ }
				}
			}, { }
		}
	} 
};

#endif /* PIN_PATCH_REALTEK_H */
