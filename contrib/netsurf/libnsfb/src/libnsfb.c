/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"
#include "nsfb.h"
#include "cursor.h"
#include "palette.h"
#include "surface.h"

/* exported interface documented in libnsfb.h */
nsfb_t*
nsfb_new(const enum nsfb_type_e surface_type)
{
    nsfb_t *newfb;
    newfb = calloc(1, sizeof(nsfb_t));
    if (newfb == NULL)
        return NULL;

    /* obtain surface routines */
    newfb->surface_rtns = nsfb_surface_get_rtns(surface_type);
    if (newfb->surface_rtns == NULL) {
        free(newfb);
        return NULL;
    }

    newfb->surface_rtns->defaults(newfb);

    return newfb;
}

/* exported interface documented in libnsfb.h */
int
nsfb_init(nsfb_t *nsfb)
{
    return nsfb->surface_rtns->initialise(nsfb);
}

/* exported interface documented in libnsfb.h */
int 
nsfb_free(nsfb_t *nsfb)
{
    int ret;

    if (nsfb->palette != NULL)
        nsfb_palette_free(nsfb->palette);

    if (nsfb->plotter_fns != NULL)
	free(nsfb->plotter_fns);

    if (nsfb->cursor != NULL)
	nsfb_cursor_destroy(nsfb->cursor);

    ret = nsfb->surface_rtns->finalise(nsfb);

    free(nsfb->surface_rtns);
    free(nsfb);

    return ret;
}

/* exported interface documented in libnsfb.h */
bool 
nsfb_event(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    return nsfb->surface_rtns->input(nsfb, event, timeout);
}

/* exported interface documented in libnsfb.h */
int 
nsfb_claim(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    return nsfb->surface_rtns->claim(nsfb, box);
}

/* exported interface documented in libnsfb.h */
int 
nsfb_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    return nsfb->surface_rtns->update(nsfb, box);
}

/* exported interface documented in libnsfb.h */
int 
nsfb_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format) 
{
    if (width <= 0)
        width = nsfb->width;        

    if (height <= 0)
        height = nsfb->height;        

    if (format == NSFB_FMT_ANY)
	    format = nsfb->format; 

    return nsfb->surface_rtns->geometry(nsfb, width, height, format);
}

/* exported interface documented in libnsfb.h */
int nsfb_set_parameters(nsfb_t *nsfb, const char *parameters)
{
    if ((parameters == NULL) || (*parameters == 0)) {
	return -1;
    }

    if (nsfb->parameters != NULL) {
	free(nsfb->parameters);
    }

    nsfb->parameters = strdup(parameters);

    return nsfb->surface_rtns->parameters(nsfb, parameters);
}

/* exported interface documented in libnsfb.h */
int 
nsfb_get_geometry(nsfb_t *nsfb, int *width, int *height, enum nsfb_format_e *format) 
{
    if (width != NULL)
        *width = nsfb->width;

    if (height != NULL)
        *height = nsfb->height;

    if (format != NULL)
        *format = nsfb->format;

    return 0;
}

/* exported interface documented in libnsfb.h */
int 
nsfb_get_buffer(nsfb_t *nsfb, uint8_t **ptr, int *linelen) 
{
    if (ptr != NULL) {
	*ptr = nsfb->ptr;
    }
    if (linelen != NULL) {
	*linelen = nsfb->linelen;
    }
    return 0;
}


/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */

