/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * This is the exported interface for the libnsfb graphics library. 
 */

#ifndef _LIBNSFB_H
#define _LIBNSFB_H 1

#include <stdint.h>

typedef struct nsfb_palette_s nsfb_palette_t;
typedef struct nsfb_cursor_s nsfb_cursor_t;
typedef struct nsfb_s nsfb_t;
typedef struct nsfb_event_s nsfb_event_t;

/** co-ordinate for plotting operations */
typedef struct nsfb_point_s {
    int x;
    int y;
} nsfb_point_t;

/** bounding box for plotting operations */
typedef struct nsfb_bbox_s {
    int x0;
    int y0;
    int x1;
    int y1;
} nsfb_bbox_t;

/** The type of framebuffer surface. */
enum nsfb_type_e {
    NSFB_SURFACE_NONE = 0, /**< No surface */
    NSFB_SURFACE_RAM, /**< RAM surface */
    NSFB_SURFACE_SDL, /**< SDL surface */
    NSFB_SURFACE_LINUX, /**< Linux framebuffer surface */
    NSFB_SURFACE_VNC, /**< VNC surface */
    NSFB_SURFACE_ABLE, /**< ABLE framebuffer surface */
    NSFB_SURFACE_X, /**< X windows surface */
    NSFB_SURFACE_WL /**< Wayland surface */
};

enum nsfb_format_e {
    NSFB_FMT_ANY = 0, /* No specific format - use surface default */
    NSFB_FMT_XBGR8888, /* 32bpp Blue Green Red */
    NSFB_FMT_XRGB8888, /* 32bpp Red Green Blue */
    NSFB_FMT_ABGR8888, /* 32bpp Alpha Blue Green Red */
    NSFB_FMT_ARGB8888, /* 32bpp Alpha Red Green Blue */
    NSFB_FMT_RGB888, /* 24 bpp Alpha Red Green Blue */
    NSFB_FMT_ARGB1555, /* 16 bpp 555 */ 
    NSFB_FMT_RGB565, /* 16 bpp 565 */ 
    NSFB_FMT_I8, /* 8bpp indexed */
    NSFB_FMT_I4, /* 4bpp indexed */
    NSFB_FMT_I1, /* black and white */
};

/** Select frontend type from a name.
 * 
 * @param name The name to select a frontend.
 * @return The surface type or NSFB_SURFACE_NONE if frontend with specified 
 *         name was not available
 */
enum nsfb_type_e nsfb_type_from_name(const char *name);

/** Create a nsfb context.
 *
 * This creates a framebuffer surface context.
 *
 * @param surface_type The type of surface to create a context for.
 */
nsfb_t *nsfb_new(const enum nsfb_type_e surface_type);

/** Initialise selected surface context.
 *
 * @param nsfb The context returned from ::nsfb_init
 */
int nsfb_init(nsfb_t *nsfb);

/** Free nsfb context.
 *
 * This shuts down and releases all resources associated with an nsfb context.
 *
 * @param nsfb The context returned from ::nsfb_new to free
 */
int nsfb_free(nsfb_t *nsfb);

/** Claim an area of screen to be redrawn.
 *
 * Informs the nsfb library that an area of screen will be directly
 * updated by the user program. This is neccisarry so the library can
 * ensure the soft cursor plotting is correctly handled. After the
 * update has been perfomed ::nsfb_update should be called.
 *
 * @param box The bounding box of the area which might be altered.
 */
int nsfb_claim(nsfb_t *nsfb, nsfb_bbox_t *box);

/** Update an area of screen which has been redrawn.
 *
 * Informs the nsfb library that an area of screen has been directly
 * updated by the user program. Some surfaces only show the update on
 * notification. The area updated does not neccisarrily have to
 * corelate with a previous ::nsfb_claim bounding box, however if the
 * redrawn area is larger than the claimed area pointer plotting
 * artifacts may occour.
 *
 * @param box The bounding box of the area which has been altered.
 */
int nsfb_update(nsfb_t *nsfb, nsfb_bbox_t *box);

/** Obtain the geometry of a nsfb context.
 *
 * @param width a variable to store the framebuffer width in or NULL
 * @param height a variable to store the framebuffer height in or NULL
 * @param format a variable to store the framebuffer format in or NULL
 */
int nsfb_get_geometry(nsfb_t *nsfb, int *width, int *height, enum nsfb_format_e *format);

/** Alter the geometry of a surface
 *
 * @param nsfb The context to alter.
 * @param width The new display width.
 * @param height The new display height.
 * @param format The desired surface format.
 */
int nsfb_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format);

/** Set parameters a surface
 *
 * Some surface types can take additional parameters for
 * attributes. For example the linux surface uses this to allow the
 * setting of a different output device
 *
 * @param nsfb The surface to alter.
 * @param parameters The parameters for the surface.
 */
int nsfb_set_parameters(nsfb_t *nsfb, const char *parameters);

/** Obtain the buffer memory base and stride. 
 *
 * @param nsfb The context to read.
 */
int nsfb_get_buffer(nsfb_t *nsfb, uint8_t **ptr, int *linelen);

/** Dump the surface to fd in PPM format  
 */
bool nsfb_dump(nsfb_t *nsfb, int fd);


#endif

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
