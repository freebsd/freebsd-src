/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <stdbool.h>
#include <stdio.h>

#include "libnsfb.h"
#include "libnsfb_plot.h"
#include "libnsfb_event.h"
#include "nsfb.h"
#include "surface.h"

#define UNUSED(x) ((x) = (x))

static int able_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format)
{
    if (nsfb->surface_priv != NULL)
        return -1; /* if were already initialised fail */

    nsfb->width = width;
    nsfb->height = height;
    nsfb->format = format;

    return 0;
}

static int able_initialise(nsfb_t *nsfb)
{
    UNUSED(nsfb);
    return 0;
}

static int able_finalise(nsfb_t *nsfb)
{
    UNUSED(nsfb);
    return 0;
}

static bool able_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    UNUSED(nsfb);
    UNUSED(event);
    UNUSED(timeout);
    return false;
}

const nsfb_surface_rtns_t able_rtns = {
    .initialise = able_initialise,
    .finalise = able_finalise,
    .input = able_input,
    .geometry = able_set_geometry,
};

NSFB_SURFACE_DEF(able, NSFB_SURFACE_ABLE, &able_rtns)
