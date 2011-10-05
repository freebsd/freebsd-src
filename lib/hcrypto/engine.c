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
#include <string.h>

#include <engine.h>

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif
#endif

struct hc_engine {
    int references;
    char *name;
    char *id;
    void (*destroy)(ENGINE *);
    const RSA_METHOD *rsa;
    const DH_METHOD *dh;
    const RAND_METHOD *rand;
};

ENGINE	*
ENGINE_new(void)
{
    ENGINE *engine;

    engine = calloc(1, sizeof(*engine));
    engine->references = 1;

    return engine;
}

int
ENGINE_free(ENGINE *engine)
{
    return ENGINE_finish(engine);
}

int
ENGINE_finish(ENGINE *engine)
{
    if (engine->references-- <= 0)
	abort();
    if (engine->references > 0)
	return 1;

    if (engine->name)
	free(engine->name);
    if (engine->id)
	free(engine->id);
    if(engine->destroy)
	(*engine->destroy)(engine);

    memset(engine, 0, sizeof(engine));
    engine->references = -1;


    free(engine);
    return 1;
}

int
ENGINE_up_ref(ENGINE *engine)
{
    if (engine->references < 0)
	abort();
    engine->references++;
    return 1;
}

int
ENGINE_set_id(ENGINE *engine, const char *id)
{
    engine->id = strdup(id);
    return (engine->id == NULL) ? 0 : 1;
}

int
ENGINE_set_name(ENGINE *engine, const char *name)
{
    engine->name = strdup(name);
    return (engine->name == NULL) ? 0 : 1;
}

int
ENGINE_set_RSA(ENGINE *engine, const RSA_METHOD *method)
{
    engine->rsa = method;
    return 1;
}

int
ENGINE_set_DH(ENGINE *engine, const DH_METHOD *method)
{
    engine->dh = method;
    return 1;
}

int
ENGINE_set_destroy_function(ENGINE *e, void (*destroy)(ENGINE *))
{
    e->destroy = destroy;
    return 1;
}

const char *
ENGINE_get_id(const ENGINE *engine)
{
    return engine->id;
}

const char *
ENGINE_get_name(const ENGINE *engine)
{
    return engine->name;
}

const RSA_METHOD *
ENGINE_get_RSA(const ENGINE *engine)
{
    return engine->rsa;
}

const DH_METHOD *
ENGINE_get_DH(const ENGINE *engine)
{
    return engine->dh;
}

const RAND_METHOD *
ENGINE_get_RAND(const ENGINE *engine)
{
    return engine->rand;
}

/*
 *
 */

#define SG_default_engine(type)			\
static ENGINE *type##_engine;			\
int						\
ENGINE_set_default_##type(ENGINE *engine)	\
{						\
    if (type##_engine)				\
	ENGINE_finish(type##_engine);		\
    type##_engine = engine;			\
    if (type##_engine)				\
	ENGINE_up_ref(type##_engine);		\
    return 1;					\
}						\
ENGINE *					\
ENGINE_get_default_##type(void)			\
{						\
    if (type##_engine)				\
	ENGINE_up_ref(type##_engine);		\
    return type##_engine;			\
}

SG_default_engine(RSA)
SG_default_engine(DH)

#undef SG_default_engine

/*
 *
 */

static ENGINE **engines;
static unsigned int num_engines;

static int
add_engine(ENGINE *engine)
{
    ENGINE **d, *dup;

    dup = ENGINE_by_id(engine->id);
    if (dup)
	return 0;

    d = realloc(engines, (num_engines + 1) * sizeof(*engines));
    if (d == NULL)
	return 1;
    engines = d;
    engines[num_engines++] = engine;

    return 1;
}

void
ENGINE_load_builtin_engines(void)
{
    ENGINE *engine;
    int ret;

    engine = ENGINE_new();
    if (engine == NULL)
	return;

    ENGINE_set_id(engine, "builtin");
    ENGINE_set_name(engine,
		    "Heimdal crypto builtin (ltm) engine version " PACKAGE_VERSION);
    ENGINE_set_RSA(engine, RSA_ltm_method());
    ENGINE_set_DH(engine, DH_ltm_method());

    ret = add_engine(engine);
    if (ret != 1)
	ENGINE_finish(engine);

#ifdef USE_HCRYPTO_TFM
    /*
     * TFM
     */

    engine = ENGINE_new();
    if (engine == NULL)
	return;

    ENGINE_set_id(engine, "tfm");
    ENGINE_set_name(engine,
		    "Heimdal crypto tfm engine version " PACKAGE_VERSION);
    ENGINE_set_RSA(engine, RSA_tfm_method());
    ENGINE_set_DH(engine, DH_tfm_method());

    ret = add_engine(engine);
    if (ret != 1)
	ENGINE_finish(engine);
#endif /* USE_HCRYPTO_TFM */

#ifdef USE_HCRYPTO_LTM
    /*
     * ltm
     */

    engine = ENGINE_new();
    if (engine == NULL)
	return;

    ENGINE_set_id(engine, "ltm");
    ENGINE_set_name(engine,
		    "Heimdal crypto ltm engine version " PACKAGE_VERSION);
    ENGINE_set_RSA(engine, RSA_ltm_method());
    ENGINE_set_DH(engine, DH_ltm_method());

    ret = add_engine(engine);
    if (ret != 1)
	ENGINE_finish(engine);
#endif

#ifdef HAVE_GMP
    /*
     * gmp
     */

    engine = ENGINE_new();
    if (engine == NULL)
	return;

    ENGINE_set_id(engine, "gmp");
    ENGINE_set_name(engine,
		    "Heimdal crypto gmp engine version " PACKAGE_VERSION);
    ENGINE_set_RSA(engine, RSA_gmp_method());

    ret = add_engine(engine);
    if (ret != 1)
	ENGINE_finish(engine);
#endif
}

ENGINE *
ENGINE_by_dso(const char *path, const char *id)
{
#ifdef HAVE_DLOPEN
    ENGINE *engine;
    void *handle;
    int ret;

    engine = calloc(1, sizeof(*engine));
    if (engine == NULL)
	return NULL;

    handle = dlopen(path, RTLD_NOW);
    if (handle == NULL) {
	/* printf("error: %s\n", dlerror()); */
	free(engine);
	return NULL;
    }

    {
	unsigned long version;
	openssl_v_check v_check;

	v_check = (openssl_v_check)dlsym(handle, "v_check");
	if (v_check == NULL) {
	    dlclose(handle);
	    free(engine);
	    return NULL;
	}

	version = (*v_check)(OPENSSL_DYNAMIC_VERSION);
	if (version == 0) {
	    dlclose(handle);
	    free(engine);
	    return NULL;
	}
    }

    {
	openssl_bind_engine bind_engine;

	bind_engine = (openssl_bind_engine)dlsym(handle, "bind_engine");
	if (bind_engine == NULL) {
	    dlclose(handle);
	    free(engine);
	    return NULL;
	}

	ret = (*bind_engine)(engine, id, NULL); /* XXX fix third arg */
	if (ret != 1) {
	    dlclose(handle);
	    free(engine);
	    return NULL;
	}
    }

    ENGINE_up_ref(engine);

    ret = add_engine(engine);
    if (ret != 1) {
	dlclose(handle);
	ENGINE_finish(engine);
	return NULL;
    }

    return engine;
#else
    return NULL;
#endif
}

ENGINE *
ENGINE_by_id(const char *id)
{
    int i;

    for (i = 0; i < num_engines; i++) {
	if (strcmp(id, engines[i]->id) == 0) {
	    ENGINE_up_ref(engines[i]);
	    return engines[i];
	}
    }
    return NULL;
}

void
ENGINE_add_conf_module(void)
{
}
