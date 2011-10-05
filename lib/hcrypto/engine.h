/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$
 */

#ifndef _HEIM_ENGINE_H
#define _HEIM_ENGINE_H 1

/* symbol renaming */
#define ENGINE_add_conf_module hc_ENGINE_add_conf_module
#define ENGINE_by_dso hc_ENGINE_by_dso
#define ENGINE_by_id hc_ENGINE_by_id
#define ENGINE_finish hc_ENGINE_finish
#define ENGINE_get_DH hc_ENGINE_get_DH
#define ENGINE_get_RSA hc_ENGINE_get_RSA
#define ENGINE_get_RAND hc_ENGINE_get_RAND
#define ENGINE_get_id hc_ENGINE_get_id
#define ENGINE_get_name hc_ENGINE_get_name
#define ENGINE_load_builtin_engines hc_ENGINE_load_builtin_engines
#define ENGINE_set_DH hc_ENGINE_set_DH
#define ENGINE_set_RSA hc_ENGINE_set_RSA
#define ENGINE_set_id hc_ENGINE_set_id
#define ENGINE_set_name hc_ENGINE_set_name
#define ENGINE_set_destroy_function hc_ENGINE_set_destroy_function
#define ENGINE_new hc_ENGINE_new
#define ENGINE_free hc_ENGINE_free
#define ENGINE_up_ref hc_ENGINE_up_ref
#define ENGINE_get_default_DH hc_ENGINE_get_default_DH
#define ENGINE_get_default_RSA hc_ENGINE_get_default_RSA
#define ENGINE_set_default_DH hc_ENGINE_set_default_DH
#define ENGINE_set_default_RSA hc_ENGINE_set_default_RSA

/*
 *
 */

typedef struct hc_engine ENGINE;

#define NID_md2			0
#define NID_md4			1
#define NID_md5			2
#define NID_sha1		4
#define NID_sha256		5

/*
 *
 */

#include <hcrypto/rsa.h>
#include <hcrypto/dsa.h>
#include <hcrypto/dh.h>
#include <hcrypto/rand.h>

#define OPENSSL_DYNAMIC_VERSION		(unsigned long)0x00020000

typedef int (*openssl_bind_engine)(ENGINE *, const char *, const void *);
typedef unsigned long (*openssl_v_check)(unsigned long);

ENGINE	*
	ENGINE_new(void);
int ENGINE_free(ENGINE *);
void	ENGINE_add_conf_module(void);
void	ENGINE_load_builtin_engines(void);
ENGINE *ENGINE_by_id(const char *);
ENGINE *ENGINE_by_dso(const char *, const char *);
int	ENGINE_finish(ENGINE *);
int	ENGINE_up_ref(ENGINE *);
int	ENGINE_set_id(ENGINE *, const char *);
int	ENGINE_set_name(ENGINE *, const char *);
int	ENGINE_set_RSA(ENGINE *, const RSA_METHOD *);
int	ENGINE_set_DH(ENGINE *, const DH_METHOD *);
int	ENGINE_set_destroy_function(ENGINE *, void (*)(ENGINE *));

const char *		ENGINE_get_id(const ENGINE *);
const char *		ENGINE_get_name(const ENGINE *);
const RSA_METHOD *	ENGINE_get_RSA(const ENGINE *);
const DH_METHOD *	ENGINE_get_DH(const ENGINE *);
const RAND_METHOD *	ENGINE_get_RAND(const ENGINE *);

int		ENGINE_set_default_RSA(ENGINE *);
ENGINE *	ENGINE_get_default_RSA(void);
int		ENGINE_set_default_DH(ENGINE *);
ENGINE *	ENGINE_get_default_DH(void);


#endif /* _HEIM_ENGINE_H */
