
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

#ifndef _HEIM_RAND_H
#define _HEIM_RAND_H 1

typedef struct RAND_METHOD RAND_METHOD;

#include <hcrypto/engine.h>

/* symbol renaming */
#define RAND_bytes hc_RAND_bytes
#define RAND_pseudo_bytes hc_RAND_pseudo_bytes
#define RAND_seed hc_RAND_seed
#define RAND_cleanup hc_RAND_cleanup
#define RAND_add hc_RAND_add
#define RAND_set_rand_method hc_RAND_set_rand_method
#define RAND_get_rand_method hc_RAND_get_rand_method
#define RAND_set_rand_engine hc_RAND_set_rand_engine
#define RAND_file_name hc_RAND_file_name
#define RAND_load_file hc_RAND_load_file
#define RAND_write_file hc_RAND_write_file
#define RAND_status hc_RAND_status
#define RAND_egd hc_RAND_egd
#define RAND_egd_bytes hc_RAND_egd_bytes
#define RAND_fortuna_method hc_RAND_fortuna_method
#define RAND_egd_method hc_RAND_egd_method
#define RAND_unix_method hc_RAND_unix_method
#define RAND_w32crypto_method hc_RAND_w32crypto_method

/*
 *
 */

struct RAND_METHOD
{
    void (*seed)(const void *, int);
    int (*bytes)(unsigned char *, int);
    void (*cleanup)(void);
    void (*add)(const void *, int, double);
    int (*pseudorand)(unsigned char *, int);
    int (*status)(void);
};

/*
 *
 */

int	RAND_bytes(void *, size_t num);
int	RAND_pseudo_bytes(void *, size_t);
void	RAND_seed(const void *, size_t);
void	RAND_cleanup(void);
void	RAND_add(const void *, size_t, double);

int	RAND_set_rand_method(const RAND_METHOD *);
const RAND_METHOD *
	RAND_get_rand_method(void);
int	RAND_set_rand_engine(ENGINE *);

const char *
	RAND_file_name(char *, size_t);
int	RAND_load_file(const char *, size_t);
int	RAND_write_file(const char *);
int	RAND_status(void);
int	RAND_egd(const char *);
int	RAND_egd_bytes(const char *, int);


const RAND_METHOD *	RAND_fortuna_method(void);
const RAND_METHOD *	RAND_unix_method(void);
const RAND_METHOD *	RAND_egd_method(void);
const RAND_METHOD *	RAND_w32crypto_method(void);

#endif /* _HEIM_RAND_H */
