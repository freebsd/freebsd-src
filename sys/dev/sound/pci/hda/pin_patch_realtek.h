/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
	{ /**** CODEC: HDA_CODEC_ALC255 ****/
		.id = HDA_CODEC_ALC255,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_X556UR_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_X540LA_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_Z550MA_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_JACK_WO_DETECT(25),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(DELL_9020M_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_JACK_WO_DETECT(25),
					PIN_PATCH_HPMIC_WO_DETECT(26),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC256 ****/
		.id = HDA_CODEC_ALC256,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(DELL_9020M_SUBVENDOR),
					PIN_SUBVENDOR(DELL_7000_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(27, "seq=1 as=5 misc=1 ctype=Analog device=Speaker loc=Internal conn=Fixed"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_X540A_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_X540SA_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_X541SA_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_X541UV_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_Z550SA_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_X705UD_SUBVENDOR),
					PIN_SUBVENDOR(ASUS_X555UB_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_MIC_INTERNAL(19),
					PIN_PATCH_STRING(25, "as=2 misc=1 color=Black ctype=1/8 device=Mic loc=Right"),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC260 ****/
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
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(HP_DC5750_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(17, "as=1 misc=1 ctype=ATAPI device=Speaker loc=Internal conn=Fixed"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(SONY_VAIO_TX_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(15, "color=Green ctype=1/8 device=Headphones loc=Rear"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(SONY_81BBID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(15,  "as=2 color=Black ctype=1/8 device=Headphones loc=Rear"),
					PIN_PATCH_STRING(16, "seq=15 as=3 ctype=1/8"),
					PIN_PATCH_NOT_APPLICABLE(17),
					PIN_PATCH_STRING(18, "as=3 misc=9 color=Red ctype=1/8 device=Mic loc=Rear"),
					PIN_PATCH_NOT_APPLICABLE(19),
					PIN_PATCH_NOT_APPLICABLE(20),
					PIN_PATCH_NOT_APPLICABLE(21),
					PIN_PATCH_NOT_APPLICABLE(22),
					PIN_PATCH_NOT_APPLICABLE(23),
					PIN_PATCH_NOT_APPLICABLE(24),
					PIN_PATCH_NOT_APPLICABLE(25),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC262 ****/
		.id = HDA_CODEC_ALC262,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(FS_H270_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(20, "as=1 misc=1 ctype=ATAPI device=Speaker loc=Onboard conn=Fixed"),
					PIN_PATCH_STRING(21, "seq=15 as=2 misc=4 color=Black ctype=1/8 device=Headphones loc=Front"),
					PIN_PATCH_STRING(22, "seq=15 as=1 misc=4 color=Black ctype=1/8 device=Headphones loc=Rear"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(FL_LB_S7110_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(21, "as=1 misc=1 ctype=Analog device=Speaker loc=Internal conn=Fixed"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(HP_Z200_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(22, "as=2 misc=1 ctype=ATAPI device=Speaker loc=Onboard conn=Fixed"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(TYAN_N6650W_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(22, "as=15 misc=1 color=White ctype=ATAPI device=AUX loc=Onboard"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LENOVO_3000_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(22, "seq=1 as=2"),
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
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_TM_6293_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(30, "as=8 misc=1 color=Black ctype=Combo device=SPDIF-out loc=Rear"),
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
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(21, "as=1 seq=15"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LENOVO_T430_SUBVENDOR),
					PIN_SUBVENDOR(LENOVO_T430S_SUBVENDOR),
					PIN_SUBVENDOR(LENOVO_X230_SUBVENDOR),
					PIN_SUBVENDOR(LENOVO_X230T_SUBVENDOR),
					PIN_SUBVENDOR(LENOVO_T431S_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_DOCK_MIC_IN(25),
					PIN_PATCH_DOCK_HP(27),
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
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_G73JW_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_SUBWOOFER(23),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(FL_1475ID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_DOCK_LINE_OUT(26),
					PIN_PATCH_DOCK_MIC_IN(27),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(FL_LB_U904_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HPMIC_WITH_DETECT(25),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(FL_LB_T731_SUBVENDOR),
					PIN_SUBVENDOR(FL_LB_E725_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(33, "seq=15 as=2 color=Black ctype=1/8 device=Headphones loc=Front"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(DELL_05F4ID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_05F5ID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_05F6ID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_JACK_WO_DETECT(25),
					PIN_PATCH_HPMIC_WO_DETECT(26),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_V5_571G_SUBVENDOR),
					PIN_SUBVENDOR(ACER_V5_122P_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_JACK_WO_DETECT(25),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_X101CH_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(24, "seq=12 as=2 misc=8 color=Black ctype=1/8 device=Mic loc=Right"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_AC700_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(18, "seq=15 as=2 misc=9 ctype=ATAPI device=Mic loc=Onboard conn=Fixed"),
					PIN_PATCH_STRING(20,  "as=1 misc=1 ctype=ATAPI device=Speaker loc=Onboard conn=Fixed"),
					PIN_PATCH_STRING(24, "as=1 misc=1 ctype=ATAPI device=Speaker loc=Onboard conn=Fixed"),
					PIN_PATCH_STRING(30, "seq=14 as=1 color=Black ctype=Digital device=SPDIF-out loc=Left"),
					PIN_PATCH_STRING(33, "seq=15 as=1 color=Black ctype=1/8 device=Headphones loc=Left"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(HP_225AID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(24, "seq=15 as=3 color=Black ctype=1/8 device=Line-in loc=Ext-Rear"),
					PIN_PATCH_STRING(27, "as=2 color=Black ctype=1/8 loc=Ext-Rear"),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC271 ****/
		.id = HDA_CODEC_ALC271,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_AO725_SUBVENDOR),
					PIN_SUBVENDOR(ACER_AO756_SUBVENDOR),
					PIN_SUBVENDOR(ACER_E1_472_SUBVENDOR),
					PIN_SUBVENDOR(ACER_E1_572_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(20, "as=1 misc=1 ctype=ATAPI device=Speaker loc=Onboard conn=Fixed"),
					PIN_PATCH_STRING(25, "as=2 misc=12 color=Pink ctype=1/8 device=Mic loc=Rear"),
					PIN_PATCH_STRING(27, "seq=15 as=2 misc=1 ctype=Analog device=Mic loc=Onboard conn=Fixed"),
					PIN_PATCH_HP_OUT(33),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC280 ****/
		.id = HDA_CODEC_ALC280,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(HP_2272ID_SUBVENDOR),
					PIN_SUBVENDOR(HP_2273ID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(27, "as=2 color=Black ctype=1/8 loc=Ext-Rear"),
					PIN_PATCH_HPMIC_WITH_DETECT(26),
					PIN_PATCH_STRING(24, "seq=15 as=3 color=Black ctype=1/8 device=Line-in loc=Ext-Rear"),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC282 ****/
		.id = HDA_CODEC_ALC282,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_V5_573G_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(18, "as=3 misc=1 ctype=Digital device=Mic loc=Internal conn=Fixed"),
					PIN_PATCH_STRING(20, "as=1 misc=1 ctype=Analog device=Speaker loc=Internal conn=Fixed"),
					PIN_PATCH_STRING(23, "seq=8 conn=None"),
					PIN_PATCH_NOT_APPLICABLE(24),
					PIN_PATCH_JACK_WO_DETECT(25),
					PIN_PATCH_NOT_APPLICABLE(26),
					PIN_PATCH_NOT_APPLICABLE(27),
					PIN_PATCH_STRING(29, "seq=13 as=2 misc=11 color=Pink ctype=DIN device=Other conn=None"),
					PIN_PATCH_NOT_APPLICABLE(30),
					PIN_PATCH_STRING(33, "seq=15 as=1 color=Black ctype=1/8 device=Headphones loc=Left"),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC285 ****/
		.id = HDA_CODEC_ALC285,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LENOVO_X120KH_SUBVENDOR),
					PIN_SUBVENDOR(LENOVO_X120QD_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(33, "seq=15 as=1 color=Black ctype=1/8 device=Headphones loc=Left"),
					{ }
				}
			}, { }
		}
	}, { /**** CODEC: HDA_CODEC_ALC286 ****/
		.id = HDA_CODEC_ALC286,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(SONY_VAIO_P11_SUBVENDOR),
					PIN_SUBVENDOR(SONY_VAIO_P13_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_JACK_WO_DETECT(25),
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
	}, { /**** CODEC: HDA_CODEC_ALC290 ****/
		.id = HDA_CODEC_ALC290,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(DELL_V5470_SUBVENDOR),
					PIN_SUBVENDOR(DELL_V5470_1_SUBVENDOR),
					PIN_SUBVENDOR(DELL_V5480_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(18, "as=4 misc=1 ctype=Digital device=Mic loc=Internal conn=Fixed"),
					PIN_PATCH_STRING(20, "as=1 misc=1 ctype=Analog device=Speaker loc=Internal conn=Fixed"),
					PIN_PATCH_STRING(21, "seq=15 as=1 color=Green ctype=1/8 device=Headphones loc=Front"),
					PIN_PATCH_STRING(23, "seq=2 as=1 misc=1 ctype=Analog device=Speaker loc=Internal conn=Fixed"),
					PIN_PATCH_JACK_WO_DETECT(26),
					{ }
				}
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
	}, { /**** CODEC: HDA_CODEC_ALC293 ****/
		.id = HDA_CODEC_ALC293,
		.patches = (struct model_pin_patch_t[]){
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(DELL_064AID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_064BID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_06D9ID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_06DAID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_06DBID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_06DDID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_06DEID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_06DFID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_06E0ID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_164AID_SUBVENDOR),
					PIN_SUBVENDOR(DELL_164BID_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HPMIC_WO_DETECT(24),
					PIN_PATCH_JACK_WO_DETECT(26),
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
			},
			{
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LENOVO_ALL_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_DOCK_LINE_OUT(23),
					PIN_PATCH_HP_OUT(33),
					{ }
				},
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
					PIN_PATCH_NOT_APPLICABLE(22),
					PIN_PATCH_NOT_APPLICABLE(24),
					PIN_PATCH_NOT_APPLICABLE(26),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(COEUS_G610P_SUBVENDOR),
					PIN_SUBVENDOR(ARIMA_W810_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_NOT_APPLICABLE(23),
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
					PIN_PATCH_HP_OUT(20),
					PIN_PATCH_SPEAKER(21),
					PIN_PATCH_BASS_SPEAKER(22),
					PIN_PATCH_NOT_APPLICABLE(23),
					PIN_PATCH_NOT_APPLICABLE(24),
					PIN_PATCH_MIC_IN(25),
					PIN_PATCH_NOT_APPLICABLE(26),
					PIN_PATCH_NOT_APPLICABLE(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_NOT_APPLICABLE(30),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(UNIWILL_9054_SUBVENDOR),
					PIN_SUBVENDOR(FS_AMILO_XI1526_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HP_OUT(20),
					PIN_PATCH_SPEAKER(21),
					PIN_PATCH_NOT_APPLICABLE(22),
					PIN_PATCH_NOT_APPLICABLE(23),
					PIN_PATCH_NOT_APPLICABLE(24),
					PIN_PATCH_MIC_IN(25),
					PIN_PATCH_NOT_APPLICABLE(26),
					PIN_PATCH_NOT_APPLICABLE(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_SPDIF_OUT(30),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(LG_LW25_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_STRING(26, "seq=15 as=4 misc=4 color=Blue ctype=1/8 device=Line-in loc=Rear"),
					PIN_PATCH_STRING(27, "seq=15 as=3 color=Green ctype=1/8 device=Headphones loc=Left"),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(UNIWILL_9070_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HP(20),
					PIN_PATCH_SPEAKER(21),
					PIN_PATCH_BASS_SPEAKER(22),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(UNIWILL_9050_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_NOT_APPLICABLE(23),
					PIN_PATCH_NOT_APPLICABLE(25),
					PIN_PATCH_NOT_APPLICABLE(27),
					PIN_PATCH_NOT_APPLICABLE(31),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_Z71V_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_SPEAKER(20),
					PIN_PATCH_HP(21),
					PIN_PATCH_NOT_APPLICABLE(22),
					PIN_PATCH_NOT_APPLICABLE(23),
					PIN_PATCH_MIC_IN(24),
					PIN_PATCH_NOT_APPLICABLE(25),
					PIN_PATCH_LINE_IN(26),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ASUS_W5A_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_HP(20),
					PIN_PATCH_NOT_APPLICABLE(21),
					PIN_PATCH_NOT_APPLICABLE(22),
					PIN_PATCH_NOT_APPLICABLE(23),
					PIN_PATCH_MIC_INTERNAL(24),
					PIN_PATCH_NOT_APPLICABLE(25),
					PIN_PATCH_NOT_APPLICABLE(26),
					PIN_PATCH_NOT_APPLICABLE(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_STRING(30, "seq=14 as=1 misc=1 color=Black ctype=ATAPI device=SPDIF-out loc=Lid-In conn=Fixed"),
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
					PIN_PATCH_LINE_OUT(20),
					PIN_PATCH_NOT_APPLICABLE(21),
					PIN_PATCH_NOT_APPLICABLE(22),
					PIN_PATCH_NOT_APPLICABLE(23),
					PIN_PATCH_STRING(24, "as=3 misc=12 color=Pink ctype=1/8 device=Mic loc=Rear"),
					PIN_PATCH_HP(25),
					PIN_PATCH_LINE_IN(26),
					PIN_PATCH_MIC_FRONT(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_NOT_APPLICABLE(30),
					PIN_PATCH_NOT_APPLICABLE(31),
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
					PIN_PATCH_LINE_OUT(20),
					PIN_PATCH_NOT_APPLICABLE(21),
					PIN_PATCH_NOT_APPLICABLE(22),
					PIN_PATCH_NOT_APPLICABLE(23),
					PIN_PATCH_STRING(24, "as=3 misc=12 color=Pink ctype=1/8 device=Mic loc=Rear"),
					PIN_PATCH_HP(25),
					PIN_PATCH_LINE_IN(26),
					PIN_PATCH_MIC_FRONT(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_STRING(30, "seq=14 as=1 misc=1 color=Black ctype=RCA device=SPDIF-out loc=Rear"),
					PIN_PATCH_NOT_APPLICABLE(31),
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
					PIN_PATCH_LINE_OUT(20),
					PIN_PATCH_NOT_APPLICABLE(21),
					PIN_PATCH_CLFE(22),
					PIN_PATCH_SURROUND(23),
					PIN_PATCH_STRING(24, "as=3 misc=12 color=Pink ctype=1/8 device=Mic loc=Rear"),
					PIN_PATCH_HP(25),
					PIN_PATCH_LINE_IN(26),
					PIN_PATCH_MIC_FRONT(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_NOT_APPLICABLE(30),
					PIN_PATCH_NOT_APPLICABLE(31),
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
					PIN_PATCH_LINE_OUT(20),
					PIN_PATCH_NOT_APPLICABLE(21),
					PIN_PATCH_CLFE(22),
					PIN_PATCH_SURROUND(23),
					PIN_PATCH_STRING(24, "as=3 misc=12 color=Pink ctype=1/8 device=Mic loc=Rear"),
					PIN_PATCH_HP(25),
					PIN_PATCH_LINE_IN(26),
					PIN_PATCH_MIC_FRONT(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_STRING(30, "seq=14 as=1 misc=1 color=Black ctype=RCA device=SPDIF-out loc=Rear"),
					PIN_PATCH_NOT_APPLICABLE(31),
					{ }
				}
			}, {
				.models = (struct pin_machine_model_t[]){
					PIN_SUBVENDOR(ACER_APFV_SUBVENDOR),
					{ }
				},
				.pin_patches = (struct pin_patch_t[]){
					PIN_PATCH_LINE_OUT(20),
					PIN_PATCH_SURROUND(21),
					PIN_PATCH_CLFE(22),
					PIN_PATCH_STRING(23, "seq=4 as=1 misc=4 color=Grey ctype=1/8 loc=Rear"),
					PIN_PATCH_STRING(24, "as=3 misc=12 color=Pink ctype=1/8 device=Mic loc=Rear"),
					PIN_PATCH_MIC_FRONT(25),
					PIN_PATCH_LINE_IN(26),
					PIN_PATCH_HP(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_NOT_APPLICABLE(30),
					PIN_PATCH_NOT_APPLICABLE(31),
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
					PIN_PATCH_LINE_OUT(20),
					PIN_PATCH_SURROUND(21),
					PIN_PATCH_CLFE(22),
					PIN_PATCH_STRING(23, "seq=4 as=1 misc=4 color=Grey ctype=1/8 loc=Rear"),
					PIN_PATCH_STRING(24, "as=3 misc=12 color=Pink ctype=1/8 device=Mic loc=Rear"),
					PIN_PATCH_MIC_FRONT(25),
					PIN_PATCH_LINE_IN(26),
					PIN_PATCH_HP(27),
					PIN_PATCH_NOT_APPLICABLE(28),
					PIN_PATCH_NOT_APPLICABLE(29),
					PIN_PATCH_STRING(30, "seq=14 as=1 misc=1 color=Black ctype=RCA device=SPDIF-out loc=Rear"),
					PIN_PATCH_NOT_APPLICABLE(31),
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
