/* $OpenBSD: ssh-pkcs11.h,v 1.7 2023/12/18 14:46:56 djm Exp $ */
/*
 * Copyright (c) 2010 Markus Friedl.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Errors for pkcs11_add_provider() */
#define	SSH_PKCS11_ERR_GENERIC			1
#define	SSH_PKCS11_ERR_LOGIN_FAIL		2
#define	SSH_PKCS11_ERR_NO_SLOTS			3
#define	SSH_PKCS11_ERR_PIN_REQUIRED		4
#define	SSH_PKCS11_ERR_PIN_LOCKED		5

int	pkcs11_init(int);
void	pkcs11_terminate(void);
int	pkcs11_add_provider(char *, char *, struct sshkey ***, char ***);
int	pkcs11_del_provider(char *);
#ifdef WITH_PKCS11_KEYGEN
struct sshkey *
	pkcs11_gakp(char *, char *, unsigned int, char *, unsigned int,
	    unsigned int, unsigned char, u_int32_t *);
struct sshkey *
	pkcs11_destroy_keypair(char *, char *, unsigned long, unsigned char,
	    u_int32_t *);
#endif

/* Only available in ssh-pkcs11-client.c so far */
int pkcs11_make_cert(const struct sshkey *,
    const struct sshkey *, struct sshkey **);
#if !defined(WITH_OPENSSL) && defined(ENABLE_PKCS11)
#undef ENABLE_PKCS11
#endif
