/*-
 * $Id:$
 */

#include "splash.h"

#if NSPLASH > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/linker.h>

#include <machine/console.h>

#include <dev/fb/fbreg.h>
#include <dev/fb/splashreg.h>

/* video adapter and image decoder */
static video_adapter_t	*splash_adp;
static splash_decoder_t	*splash_decoder;

/* decoder candidates */
static int		decoders;
static splash_decoder_t **decoder_set;
#define DECODER_ARRAY_DELTA 4

/* splash image data file */
static void		*splash_image_data;
static size_t		splash_image_size;

/* console driver callback */
static int		(*splash_callback)(int);

static int
splash_find_image(void)
{
        caddr_t image_module;
	caddr_t p;

	if (splash_image_data == NULL) {
		image_module = preload_search_by_type(SPLASH_IMAGE);
		if (image_module == NULL)
			return ENOENT;
		p = preload_search_info(image_module, MODINFO_ADDR);
		if (p == NULL)
			return ENOENT;
		splash_image_data = *(void **)p;
		p = preload_search_info(image_module, MODINFO_SIZE);
		if (p == NULL)
			return ENOENT;
		splash_image_size = *(size_t *)p;
	}
	return 0;
}

static int
splash_test(splash_decoder_t *decoder)
{
	if ((*decoder->init)(splash_adp, splash_image_data, splash_image_size))
		return ENODEV;	/* XXX */
	if (bootverbose)
		printf("splash: image decoder found: %s\n", decoder->name);
	splash_decoder = decoder;
	if (splash_callback != NULL)
		(*splash_callback)(SPLASH_INIT);
	return 0;
}

int
splash_register(splash_decoder_t *decoder)
{
	splash_decoder_t **p;
	int i;

	/* only one decoder can be active */
	if (splash_decoder != NULL)
		return ENODEV;	/* XXX */

	/* if the splash image is not in memory, abort */
	splash_find_image();
	if (bootverbose)
		printf("splash: image@%p, size:%u\n",
		       splash_image_data, splash_image_size);
	if (splash_image_data == NULL)
		return ENOENT;

	/*
	 * If the video card has aleady been initialized, test this 
	 * decoder immediately.
	 */
	if (splash_adp != NULL)
		return splash_test(decoder);

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
		if (decoder_set != NULL)
			bcopy(decoder_set, p, sizeof(*p)*decoders);
		free(decoder_set, M_DEVBUF);
		decoder_set = p;
		i = decoders++;
	}
	decoder_set[i] = decoder;
	return 0;
}

int
splash_unregister(splash_decoder_t *decoder)
{
	int error;
	int i;

	if (splash_decoder == decoder) {
		if ((error = splash_term(splash_adp)) != 0)
			return error;
	}
	for (i = 0; i < decoders; ++i) {
		if (decoder_set[i] == decoder) {
			decoder_set[i] = NULL;
			break;
		}
	}
	return 0;
}

int
splash_init(video_adapter_t *adp, int (*callback)(int))
{
	int i;

	splash_adp = adp;
	splash_callback = callback;

	/* try registered decoders with this adapter and loaded image */
	splash_decoder = NULL;
	splash_find_image();
	if (splash_image_data == NULL)
		return 0;
	for (i = 0; i < decoders; ++i) {
		if (decoder_set[i] == NULL)
			continue;
		if (splash_test(decoder_set[i]) == 0)
			break;
	}
	return 0;
}

int
splash_term(video_adapter_t *adp)
{
	int error = 0;

	if (splash_decoder != NULL) {
		if (splash_callback != NULL)
			error = (*splash_callback)(SPLASH_TERM);
		if (error == 0)
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

#endif /* NSPLASH > 0 */
