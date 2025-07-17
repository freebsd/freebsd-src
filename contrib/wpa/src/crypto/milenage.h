/*
 * UMTS AKA - Milenage algorithm (3GPP TS 35.205, .206, .207, .208)
 * Copyright (c) 2006-2007 <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef MILENAGE_H
#define MILENAGE_H

void milenage_generate(const u8 *opc, const u8 *amf, const u8 *k,
		       const u8 *sqn, const u8 *_rand, u8 *autn, u8 *ik,
		       u8 *ck, u8 *res, size_t *res_len);
int milenage_auts(const u8 *opc, const u8 *k, const u8 *_rand, const u8 *auts,
		  u8 *sqn);
int gsm_milenage(const u8 *opc, const u8 *k, const u8 *_rand, u8 *sres,
		 u8 *kc);
int milenage_check(const u8 *opc, const u8 *k, const u8 *sqn, const u8 *_rand,
		   const u8 *autn, u8 *ik, u8 *ck, u8 *res, size_t *res_len,
		   u8 *auts);
int milenage_f1(const u8 *opc, const u8 *k, const u8 *_rand,
		const u8 *sqn, const u8 *amf, u8 *mac_a, u8 *mac_s);
int milenage_f2345(const u8 *opc, const u8 *k, const u8 *_rand,
		   u8 *res, u8 *ck, u8 *ik, u8 *ak, u8 *akstar);

#endif /* MILENAGE_H */
