/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 */

#ifndef _LINUXKPI_ASM_CPU_DEVICE_ID_H_
#define	_LINUXKPI_ASM_CPU_DEVICE_ID_H_

#include <linux/bits.h>

#define	VFM_MODEL_BIT	0
#define	VFM_FAMILY_BIT	8
#define	VFM_VENDOR_BIT	16
#define	VFM_RSVD_BIT	24

#define	VFM_MODEL_MASK	GENMASK(VFM_FAMILY_BIT - 1, VFM_MODEL_BIT)
#define	VFM_FAMILY_MASK	GENMASK(VFM_VENDOR_BIT - 1, VFM_FAMILY_BIT)
#define	VFM_VENDOR_MASK	GENMASK(VFM_RSVD_BIT - 1, VFM_VENDOR_BIT)

#define	VFM_MODEL(vfm)	(((vfm) & VFM_MODEL_MASK) >> VFM_MODEL_BIT)
#define	VFM_FAMILY(vfm)	(((vfm) & VFM_FAMILY_MASK) >> VFM_FAMILY_BIT)
#define	VFM_VENDOR(vfm)	(((vfm) & VFM_VENDOR_MASK) >> VFM_VENDOR_BIT)

#define	VFM_MAKE(_vendor, _family, _model) (	\
	((_model) << VFM_MODEL_BIT) |		\
	((_family) << VFM_FAMILY_BIT) |		\
	((_vendor) << VFM_VENDOR_BIT)		\
)

#include <linux/mod_devicetable.h>
#include <asm/intel-family.h>
#include <asm/processor.h>

#define	X86_CPU_ID_FLAG_ENTRY_VALID	BIT(0)

#define	X86_MATCH_VENDORID_FAM_MODEL_STEPPINGS_FEATURE(			\
    _vendor, _family, _model, _steppings, _feature) {			\
	.vendor		= _vendor,					\
	.family		= _family,					\
	.model		= _model,					\
	.steppings	= _steppings,					\
	.feature	= _feature,					\
	.flags          = X86_CPU_ID_FLAG_ENTRY_VALID,			\
}

#define	X86_MATCH_VFM(_vfm, _data)					\
    X86_MATCH_VENDORID_FAM_MODEL_STEPPINGS_FEATURE(			\
	VFM_VENDOR(_vfm),						\
	VFM_FAMILY(_vfm),						\
	VFM_MODEL(_vfm),						\
	X86_STEPPING_ANY,						\
	X86_FEATURE_ANY)

const struct x86_cpu_id *linuxkpi_x86_match_cpu(const struct x86_cpu_id *match_array);
#define	x86_match_cpu(match_array) linuxkpi_x86_match_cpu(match_array)

#endif /* _LINUXKPI_ASM_CPU_DEVICE_ID_H_ */
