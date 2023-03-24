/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __VMMAPI_INTERNAL_H__
#define	__VMMAPI_INTERNAL_H__

struct vmctx;

struct vcpu {
	struct vmctx *ctx;
	int vcpuid;
};

#endif /* !__VMMAPI_INTERNAL_H__ */
