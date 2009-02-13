/*
 * UMTS AKA - Milenage algorithm (3GPP TS 35.205, .206, .207, .208)
 * Copyright (c) 2006 <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef MILENAGE_H
#define MILENAGE_H

void milenage_generate(const u8 *opc, const u8 *amf, const u8 *k,
		       const u8 *sqn, const u8 *_rand, u8 *autn, u8 *ik,
		       u8 *ck, u8 *res, size_t *res_len);
int milenage_auts(const u8 *opc, const u8 *k, const u8 *_rand, const u8 *auts,
		  u8 *sqn);
void gsm_milenage(const u8 *opc, const u8 *k, const u8 *_rand, u8 *sres,
		  u8 *kc);

#endif /* MILENAGE_H */
