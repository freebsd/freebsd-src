/*	$FreeBSD$	*/
/*	$OpenBSD: xform.h,v 1.8 2001/08/28 12:20:43 ben Exp $	*/

/*-
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000 Angelos D. Keromytis
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by John-Mark Gurney
 * under sponsorship of the FreeBSD Foundation and
 * Rubicon Communications, LLC (Netgate).
 *
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#ifndef _CRYPTO_XFORM_COMP_H_
#define _CRYPTO_XFORM_COMP_H_

#include <sys/types.h>

#include <opencrypto/deflate.h>
#include <opencrypto/cryptodev.h>

/* Declarations */
struct comp_algo {
	int type;
	char *name;
	size_t minlen;
	uint32_t (*compress) (uint8_t *, uint32_t, uint8_t **);
	uint32_t (*decompress) (uint8_t *, uint32_t, uint8_t **);
};

extern const struct comp_algo comp_algo_deflate;

#endif /* _CRYPTO_XFORM_COMP_H_ */
