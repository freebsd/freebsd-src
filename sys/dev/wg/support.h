/* SPDX-License-Identifier: ISC
 *
 * Copyright (C) 2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (c) 2021 Kyle Evans <kevans@FreeBSD.org>
 *
 * support.h contains code that is not _yet_ upstream in FreeBSD's main branch.
 * It is different from compat.h, which is strictly for backports.
 */

#ifndef _WG_SUPPORT
#define _WG_SUPPORT

#ifndef ck_pr_store_bool
#define ck_pr_store_bool(dst, val) ck_pr_store_8((uint8_t *)(dst), (uint8_t)(val))
#endif

#ifndef ck_pr_load_bool
#define ck_pr_load_bool(src) ((bool)ck_pr_load_8((uint8_t *)(src)))
#endif

#endif
