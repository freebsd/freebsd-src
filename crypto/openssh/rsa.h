/*	$OpenBSD: rsa.h,v 1.15 2002/03/04 17:27:39 stevesk Exp $	*/

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * RSA key generation, encryption and decryption.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#ifndef RSA_H
#define RSA_H

#include <openssl/bn.h>
#include <openssl/rsa.h>

void	 rsa_public_encrypt(BIGNUM *, BIGNUM *, RSA *);
int	 rsa_private_decrypt(BIGNUM *, BIGNUM *, RSA *);
void	 rsa_generate_additional_parameters(RSA *);

#endif				/* RSA_H */
