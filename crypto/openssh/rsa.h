/*
 *
 * rsa.h
 *
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 *
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * Created: Fri Mar  3 22:01:06 1995 ylo
 *
 * RSA key generation, encryption and decryption.
 *
 * $FreeBSD$
*/

/* RCSID("$Id: rsa.h,v 1.6 2000/04/14 10:30:32 markus Exp $"); */

#ifndef RSA_H
#define RSA_H

#include <openssl/bn.h>
#include <openssl/rsa.h>

/* Calls SSL RSA_generate_key, only copies to prv and pub */
void    rsa_generate_key(RSA * prv, RSA * pub, unsigned int bits);

/*
 * Indicates whether the rsa module is permitted to show messages on the
 * terminal.
 */
void rsa_set_verbose __P((int verbose));

int rsa_alive __P((void));

void rsa_public_encrypt __P((BIGNUM * out, BIGNUM * in, RSA * prv));
void rsa_private_decrypt __P((BIGNUM * out, BIGNUM * in, RSA * prv));

#endif				/* RSA_H */
