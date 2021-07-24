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
#ifndef PIN_PATCH_H
#define PIN_PATCH_H

#include "hdac.h"

#define PIN_SUBVENDOR(sv)		{ .id = sv }


#define PIN_PATCH_STRING(n, patchstr) {		\
	.nid = n,				\
	.type = PIN_PATCH_TYPE_STRING,		\
	.patch.string = patchstr		\
}
#define PIN_OVERRIDE(n, newvalue) {		\
	.nid = n,				\
	.type = PIN_PATCH_TYPE_OVERRIDE,	\
	.patch.override = newvalue		\
}
#define PIN_PATCH_NOT_APPLICABLE(n)		\
	PIN_PATCH_STRING(n, "as=15 misc=1 color=Black ctype=1/8 device=Speaker loc=Rear conn=None")
#define PIN_PATCH_HP_OUT(n)			\
	PIN_PATCH_STRING(n, "seq=15 as=1 color=Green ctype=1/8 device=Headphones loc=Rear")
#define PIN_PATCH_HP(n)				\
	PIN_PATCH_STRING(n, "seq=15 as=1 misc=1 color=Green ctype=1/8 device=Headphones loc=Rear")
#define PIN_PATCH_SPEAKER(n)			\
	PIN_PATCH_STRING(n, "as=2 misc=1 ctype=ATAPI loc=Onboard conn=Fixed")
#define PIN_PATCH_BASS_SPEAKER(n)		\
	PIN_PATCH_STRING(n, "as=3 misc=1 ctype=ATAPI loc=Onboard conn=Fixed")
#define PIN_PATCH_MIC_IN(n)			\
	PIN_PATCH_STRING(n, "as=5 misc=9 color=Pink ctype=1/8 device=Mic loc=Rear")
#define PIN_PATCH_MIC_INTERNAL(n)		\
	PIN_PATCH_STRING(n, "as=6 misc=1 ctype=Digital device=Mic loc=Internal conn=Fixed")
#define PIN_PATCH_MIC_FRONT(n)			\
	PIN_PATCH_STRING(n, "as=4 misc=12 color=Pink ctype=1/8 device=Mic loc=Front")
#define PIN_PATCH_LINE_IN(n)			\
	PIN_PATCH_STRING(n, "seq=1 as=3 color=Blue ctype=1/8 device=Line-in loc=Rear")
#define PIN_PATCH_LINE_OUT(n)			\
	PIN_PATCH_STRING(n, "as=1 color=Green ctype=1/8 loc=Rear")
#define PIN_PATCH_SPDIF_OUT(n)			\
	PIN_PATCH_STRING(n, "as=4 misc=1 color=Green ctype=Optical device=SPDIF-out loc=Rear")
#define PIN_PATCH_JACK_WO_DETECT(n)		\
	PIN_PATCH_STRING(n, "seq=12 as=3 misc=1 color=Pink ctype=1/8 device=Mic loc=Rear")
#define PIN_PATCH_HPMIC_WO_DETECT(n)		\
	PIN_PATCH_STRING(n, "seq=13 as=3 misc=1 color=Pink ctype=1/8 device=Mic loc=Rear")
#define PIN_PATCH_HPMIC_WITH_DETECT(n)		\
	PIN_PATCH_STRING(n, "seq=12 as=3 color=Pink ctype=1/8 device=Mic loc=Rear")
#define PIN_PATCH_CLFE(n)			\
	PIN_PATCH_STRING(n, "seq=1 as=1 misc=4 color=Black ctype=1/8 loc=Rear")
#define PIN_PATCH_SURROUND(n)			\
	PIN_PATCH_STRING(n, "seq=2 as=1 misc=4 color=Orange ctype=1/8 loc=Rear")
#define PIN_PATCH_SUBWOOFER(n)			\
	PIN_PATCH_STRING(n, "seq=1 as=1 misc=1 ctype=ATAPI device=Speaker loc=Onboard conn=Fixed")
#define PIN_PATCH_DOCK_LINE_OUT(n)		\
	PIN_PATCH_STRING(n, "seq=15 as=3 color=Black ctype=1/8 loc=Ext-Rear")
#define PIN_PATCH_DOCK_HP(n)			\
	PIN_PATCH_STRING(n, "seq=15 as=3 color=Black ctype=1/8 device=Headphones loc=Ext-Rear")
#define PIN_PATCH_DOCK_MIC_IN(n)		\
	PIN_PATCH_STRING(n, "as=4 color=Black ctype=1/8 device=Mic loc=Ext-Left")

enum {
	PIN_PATCH_TYPE_EOL,			/* end-of-list */
	PIN_PATCH_TYPE_STRING,
	PIN_PATCH_TYPE_MASK,
	PIN_PATCH_TYPE_OVERRIDE
};

struct pin_patch_t {
	nid_t nid;				/* nid to patch */
	int type;				/* patch type */
	union {
		const char *string;		/* patch string */
		uint32_t mask[2];		/* pin config mask */
		uint32_t override;		/* pin config override */
	} patch;
};

struct pin_machine_model_t {
	uint32_t id;				/* vendor machine id */
};

struct model_pin_patch_t {
	struct pin_machine_model_t *models;	/* list of machine models */
	struct pin_patch_t *pin_patches;	/* hardcoded overrides */
	void (*fixup_func)(struct hdaa_widget *); /* for future use */
};

struct hdaa_model_pin_patch_t {
	uint32_t id;				/* the hdaa id */
	struct model_pin_patch_t *patches;	/* list of machine patches */
};

#endif /* PIN_PATCH_H */
