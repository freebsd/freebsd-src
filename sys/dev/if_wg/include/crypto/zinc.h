/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef _WG_ZINC_H
#define _WG_ZINC_H

int chacha20_mod_init(void);
int poly1305_mod_init(void);
int chacha20poly1305_mod_init(void);
int blake2s_mod_init(void);
int curve25519_mod_init(void);

#endif
