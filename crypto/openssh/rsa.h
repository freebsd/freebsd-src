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

/* RCSID("$OpenBSD: rsa.h,v 1.11 2001/03/26 23:23:24 markus Exp $"); */
/* RCSID("$FreeBSD$"); */

#ifndef RSA_H
#define RSA_H

#include <openssl/bn.h>
#include <openssl/rsa.h>

void rsa_public_encrypt __P((BIGNUM * out, BIGNUM * in, RSA * prv));
int rsa_private_decrypt __P((BIGNUM * out, BIGNUM * in, RSA * prv));

void generate_additional_parameters __P((RSA *rsa));

#endif				/* RSA_H */
