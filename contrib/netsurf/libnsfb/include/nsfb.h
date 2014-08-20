/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * This is the internal interface for the libnsfb graphics library. 
 */

#ifndef _NSFB_H
#define _NSFB_H 1

#include <stdint.h>


/** NS Framebuffer context
 */
struct nsfb_s {
    int width; /**< Visible width. */
    int height; /**< Visible height. */

    char *parameters;

    enum nsfb_format_e format; /**< Framebuffer format  */
    int bpp; /**< Bits per pixel - distinct from format */

    uint8_t *ptr; /**< Base of video memory. */
    int linelen; /**< length of a video line. */

    struct nsfb_palette_s *palette; /**< palette for index modes */
    nsfb_cursor_t *cursor; /**< cursor */

    struct nsfb_surface_rtns_s *surface_rtns; /**< surface routines. */
    void *surface_priv; /**< surface opaque data. */

    nsfb_bbox_t clip; /**< current clipping rectangle for plotters */
    struct nsfb_plotter_fns_s *plotter_fns; /**< Plotter methods */
};


#endif

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
