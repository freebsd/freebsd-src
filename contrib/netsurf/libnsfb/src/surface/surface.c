/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "surface.h"
#include "plot.h"

#define MAX_SURFACES 16

#define UNUSED(x) ((x) = (x))

struct nsfb_surface_s {
    enum nsfb_type_e type;
    const nsfb_surface_rtns_t *rtns;
    const char *name;
};

static struct nsfb_surface_s surfaces[MAX_SURFACES];
static int surface_count = 0;

/* internal routine which lets surfaces register their presence at runtime */
void _nsfb_register_surface(const enum nsfb_type_e type, 
                             const nsfb_surface_rtns_t *rtns, 
                             const char *name)
{
    if (surface_count >= MAX_SURFACES)
        return; /* no space for additional surfaces */

    surfaces[surface_count].type = type;
    surfaces[surface_count].rtns = rtns;
    surfaces[surface_count].name = name;
    surface_count++;
}

/* default surface implementations */

static int surface_defaults(nsfb_t *nsfb)
{
    nsfb->width = 800;
    nsfb->height = 600;
    nsfb->format = NSFB_FMT_XRGB8888;

    /* select default sw plotters for bpp */
    select_plotters(nsfb);

    return 0;
}

static int surface_claim(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    UNUSED(nsfb);
    UNUSED(box);
    return 0;
}

static int surface_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    UNUSED(nsfb);
    UNUSED(box);
    return 0;
}

static int surface_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
    UNUSED(nsfb);
    UNUSED(cursor);
    return 0;
}

static int surface_parameters(nsfb_t *nsfb, const char *parameters)
{
    UNUSED(nsfb);
    UNUSED(parameters);
    return 0;
}

/* exported interface documented in surface.h */
nsfb_surface_rtns_t *
nsfb_surface_get_rtns(enum nsfb_type_e type)
{
    int fend_loop;
    nsfb_surface_rtns_t *rtns = NULL;

    for (fend_loop = 0; fend_loop < surface_count; fend_loop++) {
	/* surface type must match and have a initialisor, finaliser
	 * and input method 
	 */
        if ((surfaces[fend_loop].type == type) &&
            (surfaces[fend_loop].rtns->initialise != NULL) && 
	    (surfaces[fend_loop].rtns->finalise != NULL) && 
	    (surfaces[fend_loop].rtns->input != NULL) ) {
             
	    rtns = malloc(sizeof(nsfb_surface_rtns_t));
	    if (rtns == NULL) {
		continue;
	    }

	    memcpy(rtns, 
		   surfaces[fend_loop].rtns, 
		   sizeof(nsfb_surface_rtns_t));

	    /* The rest may be empty but to avoid the null check every time
	     * provide default implementations. 
	     */
	    if (rtns->defaults == NULL) {
		rtns->defaults = surface_defaults;
	    }

	    if (rtns->claim == NULL) {
		rtns->claim = surface_claim;
	    }

	    if (rtns->update == NULL) {
		rtns->update = surface_update;
	    }

	    if (rtns->cursor == NULL) {
		rtns->cursor = surface_cursor;
	    }

	    if (rtns->parameters == NULL) {
		rtns->parameters = surface_parameters;
	    }
            
            break;
        }
    }
    return rtns;
}

/* exported interface defined in libnsfb.h */
enum nsfb_type_e 
nsfb_type_from_name(const char *name)
{
    int fend_loop;

    for (fend_loop = 0; fend_loop < surface_count; fend_loop++) {
        if (strcmp(surfaces[fend_loop].name, name) == 0)
            return surfaces[fend_loop].type;
    }
    return NSFB_SURFACE_NONE;
}

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
