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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <dsa.h>

#include <roken.h>

/*
 *
 */

DSA *
DSA_new(void)
{
    DSA *dsa = calloc(1, sizeof(*dsa));
    dsa->meth = rk_UNCONST(DSA_get_default_method());
    dsa->references = 1;
    return dsa;
}

void
DSA_free(DSA *dsa)
{
    if (dsa->references <= 0)
	abort();

    if (--dsa->references > 0)
	return;

    (*dsa->meth->finish)(dsa);

#define free_if(f) if (f) { BN_free(f); }
    free_if(dsa->p);
    free_if(dsa->q);
    free_if(dsa->g);
    free_if(dsa->pub_key);
    free_if(dsa->priv_key);
    free_if(dsa->kinv);
    free_if(dsa->r);
#undef free_if

    memset(dsa, 0, sizeof(*dsa));
    free(dsa);

}

int
DSA_up_ref(DSA *dsa)
{
    return ++dsa->references;
}

/*
 *
 */

static const DSA_METHOD dsa_null_method = {
    "hcrypto null DSA"
};

const DSA_METHOD *
DSA_null_method(void)
{
    return &dsa_null_method;
}


const DSA_METHOD *dsa_default_mech = &dsa_null_method;

void
DSA_set_default_method(const DSA_METHOD *mech)
{
    dsa_default_mech = mech;
}

const DSA_METHOD *
DSA_get_default_method(void)
{
    return dsa_default_mech;
}

int
DSA_verify(int type, const unsigned char * digest, int digest_len,
	   const unsigned char *sig, int sig_len, DSA *dsa)
{
    return -1;
}
