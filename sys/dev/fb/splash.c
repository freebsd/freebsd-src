/*-
 * Copyright (c) 1999 Kazutaka YOKOTA <yokota@zodiac.mech.utsunomiya-u.ac.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/fb/splash.c,v 1.8 2000/01/29 14:42:57 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/linker.h>
#include <sys/fbio.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/splashreg.h>

/* video adapter and image decoder */
static video_adapter_t	*splash_adp;
static splash_decoder_t	*splash_decoder;

/* decoder candidates */
static int		decoders;
static splash_decoder_t **decoder_set;
#define DECODER_ARRAY_DELTA 4

/* console driver callback */
static int		(*splash_callback)(int, void *);
static void		*splash_arg;

static int
splash_find_data(splash_decoder_t *decoder)
{
        caddr_t image_module;
	caddr_t p;

	if (decoder->data_type == NULL)
		return 0;
	image_module = preload_search_by_type(decoder->data_type);
	if (image_module == NULL)
		return ENOENT;
	p = preload_search_info(image_module, MODINFO_ADDR);
	if (p == NULL)
		return ENOENT;
	decoder->data = *(void **)p;
	p = preload_search_info(image_module, MODINFO_SIZE);
	if (p == NULL)
		return ENOENT;
	decoder->data_size = *(size_t *)p;
	if (bootverbose)
		printf("splash: image@%p, size:%lu\n",
		       (void *)decoder->data, (long)decoder->data_size);
	return 0;
}

static int
splash_test(splash_decoder_t *decoder)
{
	if (splash_find_data(decoder))
		return ENOENT;	/* XXX */
	if (*decoder->init && (*decoder->init)(splash_adp)) {
		decoder->data = NULL;
		decoder->data_size = 0;
		return ENODEV;	/* XXX */
	}
	if (bootverbose)
		printf("splash: image decoder found: %s\n", decoder->name);
	return 0;
}

static void
splash_new(splash_decoder_t *decoder)
{
	splash_decoder = decoder;
	if (splash_callback != NULL)
		(*splash_callback)(SPLASH_INIT, splash_arg);
}

int
splash_register(splash_decoder_t *decoder)
{
	splash_decoder_t **p;
	int error;
	int i;

	if (splash_adp != NULL) {
		/*
		 * If the video card has aleady been initialized, test
		 * this decoder immediately.
		 */
		error = splash_test(decoder);
		if (error == 0) {
			/* replace the current decoder with new one */
			if (splash_decoder != NULL)
				error = splash_term(splash_adp);
			if (error == 0)
				splash_new(decoder);
		}
		return error;
	} else {
		/* register the decoder for later use */
		for (i = 0; i < decoders; ++i) {
			if (decoder_set[i] == NULL)
				break;
		}
		if ((i >= decoders) && (decoders % DECODER_ARRAY_DELTA) == 0) {
			p = malloc(sizeof(*p)*(decoders + DECODER_ARRAY_DELTA),
			   	M_DEVBUF, M_NOWAIT);
			if (p == NULL)
				return ENOMEM;
			if (decoder_set != NULL) {
				bcopy(decoder_set, p, sizeof(*p)*decoders);
				free(decoder_set, M_DEVBUF);
			}
			decoder_set = p;
			i = decoders++;
		}
		decoder_set[i] = decoder;
	}

	return 0;
}

int
splash_unregister(splash_decoder_t *decoder)
{
	int error;

	if (splash_decoder == decoder) {
		if ((error = splash_term(splash_adp)) != 0)
			return error;
	}
	return 0;
}

int
splash_init(video_adapter_t *adp, int (*callback)(int, void *), void *arg)
{
	int i;

	splash_adp = adp;
	splash_callback = callback;
	splash_arg = arg;

	splash_decoder = NULL;
	for (i = 0; i < decoders; ++i) {
		if (decoder_set[i] == NULL)
			continue;
		if (splash_test(decoder_set[i]) == 0) {
			splash_new(decoder_set[i]);
			break;
		}
		decoder_set[i] = NULL;
	}
	for (++i; i < decoders; ++i) {
		decoder_set[i] = NULL;
	}
	return 0;
}

int
splash_term(video_adapter_t *adp)
{
	int error = 0;

	if (splash_adp != adp)
		return EINVAL;
	if (splash_decoder != NULL) {
		if (splash_callback != NULL)
			error = (*splash_callback)(SPLASH_TERM, splash_arg);
		if (error == 0 && splash_decoder->term)
			error = (*splash_decoder->term)(adp);
		if (error == 0)
			splash_decoder = NULL;
	}
	return error;
}

int
splash(video_adapter_t *adp, int on)
{
	if (splash_decoder != NULL)
		return (*splash_decoder->splash)(adp, on);
	return ENODEV;
}
