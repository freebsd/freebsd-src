/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#define _XOPEN_SOURCE 500

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <linux/input.h>
#include <wayland-client.h>

#include "libnsfb.h"
#include "libnsfb_event.h"
#include "libnsfb_plot.h"
#include "libnsfb_plot_util.h"

#include "nsfb.h"
#include "surface.h"
#include "plot.h"
#include "cursor.h"

struct wld_event {
    struct wld_event *next;

    nsfb_event_t event;
};

/** structure for display, registry and other global objects that
 * should be cached when connecting to a wayland instance
 */
struct wld_connection {
    struct wl_display *display; /**< connection object */
    struct wl_registry *registry; /**< registry object */

    /** compositor object, available once teh registry messages have
     * been processed
     */
    struct wl_compositor *compositor;

    /** shell object, available once the registry messages have been
     * processed
     */
    struct wl_shell *shell;

    /** shared memory object, available once the registry messages
     * have been processed
     */
    struct wl_shm *shm;

    /** shared memory formats available */
    uint32_t shm_formats;

    /** list of input seats */
    struct wl_list input_list;

    /** event queue */
    struct wld_event *event_head;
    struct wld_event *event_tail;
};

/** wayland input seat */
struct wld_input {
    struct wl_list link; /**< input list */

    struct wld_connection* connection; /**< connection to wayland server */

    struct wl_seat *seat; /**< The seat object */

    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;


};

/** wayland window encompasing the display and shell surfaces */
struct wld_window {
    struct wld_connection* connection; /**< connection to wayland server */

    struct wl_surface *surface; /**< drawing surface object */
    struct wl_shell_surface *shell_surface; /**< shell surface object */

    int width, height;
};

struct wld_shm_buffer {
    struct wl_buffer *buffer; /**< wayland buffer object */
    void *data; /**< mapped memory */
    int size; /**< size of mapped memory */
    bool inuse; /**< flag to indicate if the buffer has been released
		 * after commit to a surface.
		 */
};


typedef struct wldstate_s {
    struct wld_connection* connection; /**< connection to wayland server */
    struct wld_window *window;
    struct wld_shm_buffer *shm_buffer;
} wldstate_t;


#if 0

#if defined(NSFB_NEED_HINTS_ALLOC)
static xcb_size_hints_t *
xcb_alloc_size_hints(void)
{
    return calloc(1, sizeof(xcb_size_hints_t));
}

static void
xcb_free_size_hints(xcb_size_hints_t *hints)
{
    free(hints);
}
#endif

#if defined(NSFB_NEED_ICCCM_API_PREFIX)
#define xcb_size_hints_set_max_size xcb_icccm_size_hints_set_max_size
#define xcb_size_hints_set_min_size xcb_icccm_size_hints_set_min_size
#define xcb_set_wm_size_hints       xcb_icccm_set_wm_size_hints
#endif

#if (NSFB_XCBPROTO_MAJOR_VERSION > 1) || \
    (NSFB_XCBPROTO_MAJOR_VERSION == 1 && NSFB_XCBPROTO_MINOR_VERSION >= 6)
#define WM_NORMAL_HINTS XCB_ATOM_WM_NORMAL_HINTS
#endif

#define X_BUTTON_LEFT 1
#define X_BUTTON_MIDDLE 2
#define X_BUTTON_RIGHT 3
#define X_BUTTON_WHEELUP 4
#define X_BUTTON_WHEELDOWN 5

typedef struct xstate_s {
    xcb_connection_t *connection; /* The x server connection */
    xcb_screen_t *screen; /* The screen to put the window on */
    xcb_key_symbols_t *keysymbols; /* keysym mappings */

    xcb_shm_segment_info_t shminfo;

    xcb_image_t *image; /* The X image buffer */

    xcb_window_t window; /* The handle to the window */
    xcb_pixmap_t pmap; /* The handle to the backing pixmap */
    xcb_gcontext_t gc; /* The handle to the pixmap plotting graphics context */
    xcb_shm_seg_t segment; /* The handle to the image shared memory */
} xstate_t;

/* X keyboard codepage to nsfb mapping*/
static enum nsfb_key_code_e XCSKeyboardMap[256] = {
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_BACKSPACE, /* XK_BackSpace */
    NSFB_KEY_TAB, /* XK_Tab */
    NSFB_KEY_UNKNOWN, /* XK_Linefeed */
    NSFB_KEY_CLEAR, /* XK_Clear */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_RETURN, /* XK_Return */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x10 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_PAUSE, /* XK_Pause */
    NSFB_KEY_UNKNOWN, /* XK_Scroll_Lock */
    NSFB_KEY_UNKNOWN, /* XK_Sys_Req */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_ESCAPE, /* XK_Escape */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x20 */
    NSFB_KEY_UNKNOWN, /* XK_Multi_key */
    NSFB_KEY_UNKNOWN, /* XK_Kanji */
    NSFB_KEY_UNKNOWN, /* XK_Muhenkan */
    NSFB_KEY_UNKNOWN, /* XK_Henkan_Mode */
    NSFB_KEY_UNKNOWN, /* XK_Henkan */
    NSFB_KEY_UNKNOWN, /* XK_Romaji */
    NSFB_KEY_UNKNOWN, /* XK_Hiragana*/
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x30 */
    NSFB_KEY_UNKNOWN, /* XK_Eisu_toggle */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_Codeinput */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_SingleCandidate */
    NSFB_KEY_UNKNOWN, /* XK_MultipleCandidate */
    NSFB_KEY_UNKNOWN, /* XK_PreviousCandidate */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x40 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x50 */
    NSFB_KEY_HOME, /* XK_Home */
    NSFB_KEY_LEFT, /* XK_Left */
    NSFB_KEY_UP, /* XK_Up */
    NSFB_KEY_RIGHT, /* XK_Right */
    NSFB_KEY_DOWN, /* XK_Down */
    NSFB_KEY_PAGEUP, /* XK_Page_Up */
    NSFB_KEY_PAGEDOWN, /* XK_Page_Down */
    NSFB_KEY_END, /* XK_End */
    NSFB_KEY_UNKNOWN, /* XK_Begin */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x60 */
    NSFB_KEY_UNKNOWN, /* XK_Select */
    NSFB_KEY_UNKNOWN, /* XK_Print*/
    NSFB_KEY_UNKNOWN, /* XK_Execute*/
    NSFB_KEY_UNKNOWN, /* XK_Insert*/
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_Undo*/
    NSFB_KEY_UNKNOWN, /* XK_Redo */
    NSFB_KEY_UNKNOWN, /* XK_Menu */
    NSFB_KEY_UNKNOWN, /* XK_Find */
    NSFB_KEY_UNKNOWN, /* XK_Cancel */
    NSFB_KEY_UNKNOWN, /* XK_Help */
    NSFB_KEY_UNKNOWN, /* XK_Break*/
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x70 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_Mode_switch */
    NSFB_KEY_UNKNOWN, /* XK_Num_Lock */
    /* 0x80 */
    NSFB_KEY_UNKNOWN, /* XK_KP_Space */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_Tab */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_Enter */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0x90 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_F1*/
    NSFB_KEY_UNKNOWN, /* XK_KP_F2*/
    NSFB_KEY_UNKNOWN, /* XK_KP_F3*/
    NSFB_KEY_UNKNOWN, /* XK_KP_F4*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Home*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Left*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Up*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Right*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Down*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Page_Up*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Page_Down*/
    NSFB_KEY_UNKNOWN, /* XK_KP_End*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Begin*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Insert*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Delete*/
    /* 0xa0 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_Multiply*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Add*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Separator*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Subtract*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Decimal*/
    NSFB_KEY_UNKNOWN, /* XK_KP_Divide*/
    /* 0xb0 */
    NSFB_KEY_UNKNOWN, /* XK_KP_0 */
    NSFB_KEY_UNKNOWN, /* XK_KP_1 */
    NSFB_KEY_UNKNOWN, /* XK_KP_2 */
    NSFB_KEY_UNKNOWN, /* XK_KP_3 */
    NSFB_KEY_UNKNOWN, /* XK_KP_4 */
    NSFB_KEY_UNKNOWN, /* XK_KP_5 */
    NSFB_KEY_UNKNOWN, /* XK_KP_6 */
    NSFB_KEY_UNKNOWN, /* XK_KP_7 */
    NSFB_KEY_UNKNOWN, /* XK_KP_8 */
    NSFB_KEY_UNKNOWN, /* XK_KP_9 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_F1, /* XK_F1 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* XK_KP_Equal */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_F2, /* XK_F2 */
    /* 0xc0 */
    NSFB_KEY_F3, /* XK_F3*/
    NSFB_KEY_F4, /* XK_F4*/
    NSFB_KEY_F5, /* XK_F5*/
    NSFB_KEY_F6, /* XK_F6*/
    NSFB_KEY_F7, /* XK_F7*/
    NSFB_KEY_F8, /* XK_F8*/
    NSFB_KEY_F9, /* XK_F9*/
    NSFB_KEY_F10, /* XK_F10*/
    NSFB_KEY_F11, /* XK_F11*/
    NSFB_KEY_F12, /* XK_F12*/
    NSFB_KEY_F13, /* XK_F13 */
    NSFB_KEY_F14, /* XK_F14 */
    NSFB_KEY_F15, /* XK_F15 */
    NSFB_KEY_UNKNOWN, /* XK_F16 */
    NSFB_KEY_UNKNOWN, /* XK_F17 */
    NSFB_KEY_UNKNOWN, /* XK_F18*/
    /* 0xd0 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    /* 0xe0 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_LSHIFT, /* XK_Shift_L*/
    NSFB_KEY_RSHIFT, /* XK_Shift_R*/
    NSFB_KEY_UNKNOWN, /* XK_Control_L*/
    NSFB_KEY_UNKNOWN, /* XK_Control_R*/
    NSFB_KEY_UNKNOWN, /* XK_Caps_Lock*/
    NSFB_KEY_UNKNOWN, /* XK_Shift_Lock*/
    NSFB_KEY_UNKNOWN, /* XK_Meta_L*/
    NSFB_KEY_UNKNOWN, /* XK_Meta_R*/
    NSFB_KEY_UNKNOWN, /* XK_Alt_L */
    NSFB_KEY_UNKNOWN, /* XK_Alt_R*/
    NSFB_KEY_UNKNOWN, /* XK_Super_L*/
    NSFB_KEY_UNKNOWN, /* XK_Super_R*/
    NSFB_KEY_UNKNOWN, /* XK_Hyper_L*/
    NSFB_KEY_UNKNOWN, /* XK_Hyper_R*/
    NSFB_KEY_UNKNOWN, /* */
    /* 0xf0 */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
    NSFB_KEY_UNKNOWN, /* */
};

/* Convert an X keysym into a nsfb key code.
 *
 * Our approach is primarily to assume both codings are roughly ascii like.
 *
 * The Keysyms are defined in X11/keysymdef.h and our mapping for the
 * "keyboard" keys i.e. those from code set 255 is from there
 */
static enum nsfb_key_code_e
xkeysym_to_nsfbkeycode(xcb_keysym_t ks)
{
    enum nsfb_key_code_e nsfb_key = NSFB_KEY_UNKNOWN;
    uint8_t codeset = (ks & 0xFF00) >> 8;
    uint8_t chrcode = ks & 0xFF;

    if (ks != XCB_NO_SYMBOL) {

	switch (codeset) {
	case 0x00: /* Latin 1 */
	case 0x01: /* Latin 2 */
	case 0x02: /* Latin 3 */
	case 0x03: /* Latin 4 */
	case 0x04: /* Katakana */
	case 0x05: /* Arabic */
	case 0x06: /* Cyrillic */
	case 0x07: /* Greek */
	case 0x08: /* Technical */
	case 0x0A: /* Publishing */
	case 0x0C: /* Hebrew */
	case 0x0D: /* Thai */
	    /* this is somewhat incomplete, but the nsfb codes are lined up on
	     * the ascii codes and x seems to have done similar
	     */
	    nsfb_key = (enum nsfb_key_code_e)chrcode;
	    break;

	case 0xFF: /* Keyboard */
	    nsfb_key = XCSKeyboardMap[chrcode];
	    break;
	}
    }

    return nsfb_key;
}
/*
  static void
  set_palette(nsfb_t *nsfb)
  {
  X_Surface *x_screen = nsfb->surface_priv;
  X_Color palette[256];
  int rloop, gloop, bloop;
  int loop = 0;

  // build a linear R:3 G:3 B:2 colour cube palette.
  for (rloop = 0; rloop < 8; rloop++) {
  for (gloop = 0; gloop < 8; gloop++) {
  for (bloop = 0; bloop < 4; bloop++) {
  palette[loop].r = (rloop << 5) | (rloop << 2) | (rloop >> 1);
  palette[loop].g = (gloop << 5) | (gloop << 2) | (gloop >> 1);
  palette[loop].b = (bloop << 6) | (bloop << 4) | (bloop << 2) | (bloop);
  nsfb->palette[loop] = palette[loop].r |
  palette[loop].g << 8 |
  palette[loop].b << 16;
  loop++;
  }
  }
  }

  // Set palette
  //X_SetColors(x_screen, palette, 0, 256);

  }
*/
static int
update_pixmap(xstate_t *xstate, int x, int y, int width, int height)
{
    if (xstate->shminfo.shmseg == 0) {
	/* not using shared memory */
	xcb_put_image(xstate->connection,
		      xstate->image->format,
		      xstate->pmap,
		      xstate->gc,
		      xstate->image->width,
		      height,
		      0,
		      y,
		      0,
		      xstate->image->depth,
		      (height) * xstate->image->stride,
		      xstate->image->data + (y * xstate->image->stride));
    } else {
	/* shared memory */
	xcb_image_shm_put(xstate->connection,
			  xstate->pmap,
			  xstate->gc,
			  xstate->image,
			  xstate->shminfo,
			  x,y,
			  x,y,
			  width,height,0);
    }

    return 0;
}

static int
update_and_redraw_pixmap(xstate_t *xstate, int x, int y, int width, int height)
{
    update_pixmap(xstate, x, y, width, height);

    xcb_copy_area(xstate->connection,
		  xstate->pmap,
		  xstate->window,
		  xstate->gc,
		  x, y,
		  x, y,
		  width, height);

    xcb_flush(xstate->connection);

    return 0;
}


static bool
xcopy(nsfb_t *nsfb, nsfb_bbox_t *srcbox, nsfb_bbox_t *dstbox)
{
    xstate_t *xstate = nsfb->surface_priv;
    nsfb_bbox_t allbox;
    struct nsfb_cursor_s *cursor = nsfb->cursor;
    uint8_t *srcptr;
    uint8_t *dstptr;
    int srcx = srcbox->x0;
    int srcy = srcbox->y0;
    int dstx = dstbox->x0;
    int dsty = dstbox->y0;
    int width = dstbox->x1 - dstbox->x0;
    int height = dstbox->y1 - dstbox->y0;
    int hloop;

    nsfb_plot_add_rect(srcbox, dstbox, &allbox);

    /* clear the cursor if its within the region to be altered */
    if ((cursor != NULL) &&
	(cursor->plotted == true) &&
	(nsfb_plot_bbox_intersect(&allbox, &cursor->loc))) {

	nsfb_cursor_clear(nsfb, cursor);
	update_pixmap(xstate,
		      cursor->savloc.x0,
		      cursor->savloc.y0,
		      cursor->savloc.x1 - cursor->savloc.x0,
		      cursor->savloc.y1 - cursor->savloc.y0);

	/* must sync here or local framebuffer and remote pixmap will not be
	 * consistant
	 */
	xcb_aux_sync(xstate->connection);

    }

    /* copy the area on the server */
    xcb_copy_area(xstate->connection,
		  xstate->pmap,
		  xstate->pmap,
		  xstate->gc,
		  srcbox->x0,
		  srcbox->y0,
		  dstbox->x0,
		  dstbox->y0,
		  srcbox->x1 - srcbox->x0,
		  srcbox->y1 - srcbox->y0);

    /* do the copy in the local memory too */
    srcptr = (nsfb->ptr +
	      (srcy * nsfb->linelen) +
	      ((srcx * nsfb->bpp) / 8));

    dstptr = (nsfb->ptr +
	      (dsty * nsfb->linelen) +
	      ((dstx * nsfb->bpp) / 8));

    if (width == nsfb->width) {
	/* take shortcut and use memmove */
	memmove(dstptr, srcptr, (width * height * nsfb->bpp) / 8);
    } else {
	if (srcy > dsty) {
	    for (hloop = height; hloop > 0; hloop--) {
		memmove(dstptr, srcptr, (width * nsfb->bpp) / 8);
		srcptr += nsfb->linelen;
		dstptr += nsfb->linelen;
	    }
	} else {
	    srcptr += height * nsfb->linelen;
	    dstptr += height * nsfb->linelen;
	    for (hloop = height; hloop > 0; hloop--) {
		srcptr -= nsfb->linelen;
		dstptr -= nsfb->linelen;
		memmove(dstptr, srcptr, (width * nsfb->bpp) / 8);
	    }
	}
    }

    if ((cursor != NULL) &&
	(cursor->plotted == false)) {
	nsfb_cursor_plot(nsfb, cursor);
    }

    /* update the x window */
    xcb_copy_area(xstate->connection,
		  xstate->pmap,
		  xstate->window,
		  xstate->gc,
		  dstx, dsty,
		  dstx, dsty,
		  width, height);

    return true;

}

#endif


static int
wld_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format)
{
    if (nsfb->surface_priv != NULL)
	return -1; /* if were already initialised fail */

    nsfb->width = width;
    nsfb->height = height;
    nsfb->format = format;

    /* select default sw plotters for format */
    select_plotters(nsfb);

}

#if 0


static xcb_format_t *
find_format(xcb_connection_t * c, uint8_t depth, uint8_t bpp)
{
    const xcb_setup_t *setup = xcb_get_setup(c);
    xcb_format_t *fmt = xcb_setup_pixmap_formats(setup);
    xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(setup);

    for(; fmt != fmtend; ++fmt) {
	if((fmt->depth == depth) && (fmt->bits_per_pixel == bpp)) {
	    return fmt;
	}
    }
    return 0;
}

static xcb_image_t *
create_shm_image(xstate_t *xstate, int width, int height, int bpp)
{
    const xcb_setup_t *setup = xcb_get_setup(xstate->connection);
    unsigned char *image_data;
    xcb_format_t *fmt;
    int depth = bpp;
    uint32_t image_size;
    int shmid;

    xcb_shm_query_version_reply_t *rep;
    xcb_shm_query_version_cookie_t ck;
    xcb_void_cookie_t shm_attach_cookie;
    xcb_generic_error_t *generic_error;

    ck = xcb_shm_query_version(xstate->connection);
    rep = xcb_shm_query_version_reply(xstate->connection, ck , NULL);
    if (!rep) {
	fprintf (stderr, "Server has no shm support.\n");
	return NULL;
    }

    if ((rep->major_version < 1) ||
	(rep->major_version == 1 && rep->minor_version == 0)) {
	fprintf(stderr, "server SHM support is insufficient.\n");
	free(rep);
	return NULL;
    }
    free(rep);

    if (bpp == 32)
	depth = 24;

    fmt = find_format(xstate->connection, depth, bpp);
    if (fmt == NULL)
	return NULL;

    /* doing it this way ensures we deal with bpp smaller than 8 */
    image_size = (bpp * width * height) >> 3;

    /* get the shared memory segment */
    shmid = shmget(IPC_PRIVATE, image_size, IPC_CREAT|0777);
    if (shmid == -1)
	return NULL;

    xstate->shminfo.shmid = shmid;

    xstate->shminfo.shmaddr = shmat(xstate->shminfo.shmid, 0, 0);
    image_data = xstate->shminfo.shmaddr;

    xstate->shminfo.shmseg = xcb_generate_id(xstate->connection);
    shm_attach_cookie = xcb_shm_attach_checked(xstate->connection,
					       xstate->shminfo.shmseg,
					       xstate->shminfo.shmid,
					       0);
    generic_error = xcb_request_check(xstate->connection, shm_attach_cookie);

    /* either there is an error and the shm us no longer needed, or it now
     * belongs to the x server - regardless release local reference to shared
     * memory segment
     */
    shmctl(xstate->shminfo.shmid, IPC_RMID, 0);

    if (generic_error != NULL) {
	/* unable to attach shm */
	xstate->shminfo.shmseg = 0;

	free(generic_error);
	return NULL;
    }


    return xcb_image_create(width,
			    height,
			    XCB_IMAGE_FORMAT_Z_PIXMAP,
			    fmt->scanline_pad,
			    fmt->depth,
			    fmt->bits_per_pixel,
			    0,
			    setup->image_byte_order,
			    XCB_IMAGE_ORDER_LSB_FIRST,
			    image_data,
			    image_size,
			    image_data);
}


static xcb_image_t *
create_image(xcb_connection_t *c, int width, int height, int bpp)
{
    const xcb_setup_t *setup = xcb_get_setup(c);
    unsigned char *image_data;
    xcb_format_t *fmt;
    int depth = bpp;
    uint32_t image_size;

    if (bpp == 32)
	depth = 24;

    fmt = find_format(c, depth, bpp);
    if (fmt == NULL)
	return NULL;

    /* doing it this way ensures we deal with bpp smaller than 8 */
    image_size = (bpp * width * height) >> 3;

    image_data = calloc(1, image_size);
    if (image_data == NULL)
	return NULL;

    return xcb_image_create(width,
			    height,
			    XCB_IMAGE_FORMAT_Z_PIXMAP,
			    fmt->scanline_pad,
			    fmt->depth,
			    fmt->bits_per_pixel,
			    0,
			    setup->image_byte_order,
			    XCB_IMAGE_ORDER_LSB_FIRST,
			    image_data,
			    image_size,
			    image_data);
}

/**
 * Create a blank cursor.
 * The empty pixmaps is leaked.
 *
 * @param conn xcb connection
 * @param scr xcb XCB screen
 */
static xcb_cursor_t
create_blank_cursor(xcb_connection_t *conn, const xcb_screen_t *scr)
{
    xcb_cursor_t cur = xcb_generate_id(conn);
    xcb_pixmap_t pix = xcb_generate_id(conn);
    xcb_void_cookie_t ck;
    xcb_generic_error_t *err;

    ck = xcb_create_pixmap_checked (conn, 1, pix, scr->root, 1, 1);
    err = xcb_request_check (conn, ck);
    if (err) {
	fprintf (stderr, "Cannot create pixmap: %d", err->error_code);
	free (err);
    }
    ck = xcb_create_cursor_checked (conn, cur, pix, pix, 0, 0, 0, 0, 0, 0, 0, 0);
    err = xcb_request_check (conn, ck);
    if (err) {
	fprintf (stderr, "Cannot create cursor: %d", err->error_code);
	free (err);
    }
    return cur;
}
#endif


/** shared memory interface format available callback
 *
 * @param ctx context from when listener was added.
 * @param wl_shm The shared memory object.
 * @param format The available shared memory format. Found in
 *                 wayland-client-protocol.h there are currently only two
 *                 formats available (A|X)RGB8888 so using a bitfield is ok.
 */
static void
shm_format(void *ctx, struct wl_shm *wl_shm, uint32_t format)
{
    struct wld_connection* connection = ctx;

    connection->shm_formats |= (1 << format);
}

/** shared memory interface callback handlers */
struct wl_shm_listener shm_listenter = {
	shm_format
};

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface,
		     wl_fixed_t sx_w, wl_fixed_t sy_w)
{
#if 0
	struct input *input = data;
	struct window *window;
	struct widget *widget;
	float sx = wl_fixed_to_double(sx_w);
	float sy = wl_fixed_to_double(sy_w);

	if (!surface) {
		/* enter event for a window we've just destroyed */
		return;
	}

	input->display->serial = serial;
	input->pointer_enter_serial = serial;
	input->pointer_focus = wl_surface_get_user_data(surface);
	window = input->pointer_focus;

	if (window->resizing) {
		window->resizing = 0;
		/* Schedule a redraw to free the pool */
		window_schedule_redraw(window);
	}

	input->sx = sx;
	input->sy = sy;

	widget = window_find_widget(window, sx, sy);
	input_set_focus_widget(input, widget, sx, sy);

#endif
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface)
{
#if 0
	struct input *input = data;

	input->display->serial = serial;
	input_remove_pointer_focus(input);
#endif
}

static void
enqueue_wld_event(struct wld_connection *connection, struct wld_event* event)
{
    event->next = NULL;
    if (connection->event_tail == NULL) {
	connection->event_head = event;
	connection->event_tail = event;
    } else {
	connection->event_tail->next = event;
	connection->event_tail = event;
    }
}

static struct wld_event*
dequeue_wld_event(struct wld_connection *connection)
{
    struct wld_event* event = connection->event_head;

    if (event != NULL) {
	connection->event_head = event->next;
	if (connection->event_head == NULL) {
	    connection->event_tail = connection->event_head;
	}
    }
    return event;
}

static void
pointer_handle_motion(void *data,
		      struct wl_pointer *pointer,
		      uint32_t time,
		      wl_fixed_t sx_w,
		      wl_fixed_t sy_w)
{
    struct wld_input *input = data;
    struct wld_event *event;

    event = calloc(1, sizeof(struct wld_event));

    event->event.type = NSFB_EVENT_MOVE_ABSOLUTE;
    event->event.value.vector.x = wl_fixed_to_int(sx_w);
    event->event.value.vector.y = wl_fixed_to_int(sy_w);
    event->event.value.vector.z = 0;

    enqueue_wld_event(input->connection, event);
}

static void
pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial,
		      uint32_t time, uint32_t button, uint32_t state_w)
{
    struct wld_input *input = data;
    struct wld_event *event;
    enum wl_pointer_button_state state = state_w;

    event = calloc(1, sizeof(struct wld_event));

    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
	event->event.type = NSFB_EVENT_KEY_DOWN;
    } else {
	event->event.type = NSFB_EVENT_KEY_UP;
    }

    switch (button) {
    case BTN_LEFT:
	event->event.value.keycode = NSFB_KEY_MOUSE_1;
	break;
    case BTN_MIDDLE:
	event->event.value.keycode = NSFB_KEY_MOUSE_2;
	break;
    case BTN_RIGHT:
	event->event.value.keycode = NSFB_KEY_MOUSE_3;
	break;
    case BTN_FORWARD:
	event->event.value.keycode = NSFB_KEY_MOUSE_4;
	break;
    case BTN_BACK:
	event->event.value.keycode = NSFB_KEY_MOUSE_5;
	break;
    }

    enqueue_wld_event(input->connection, event);


#if 0
	struct input *input = data;
	struct widget *widget;
	enum wl_pointer_button_state state = state_w;

	input->display->serial = serial;
	if (input->focus_widget && input->grab == NULL &&
	    state == WL_POINTER_BUTTON_STATE_PRESSED)
		input_grab(input, input->focus_widget, button);

	widget = input->grab;
	if (widget && widget->button_handler)
		(*widget->button_handler)(widget,
					  input, time,
					  button, state,
					  input->grab->user_data);

	if (input->grab && input->grab_button == button &&
	    state == WL_POINTER_BUTTON_STATE_RELEASED)
		input_ungrab(input);
#endif
}

static void
pointer_handle_axis(void *data, struct wl_pointer *pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
#if 0
	struct input *input = data;
	struct widget *widget;

	widget = input->focus_widget;
	if (input->grab)
		widget = input->grab;
	if (widget && widget->axis_handler)
		(*widget->axis_handler)(widget,
					input, time,
					axis, value,
					widget->user_data);
#endif
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static void
seat_handle_capabilities(void *data,
			 struct wl_seat *seat,
			 enum wl_seat_capability caps)
{
    struct wld_input *input = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
	input->pointer = wl_seat_get_pointer(seat);

	wl_pointer_add_listener(input->pointer, &pointer_listener, input);

    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->pointer) {

	wl_pointer_destroy(input->pointer);
	input->pointer = NULL;
    }

#if 0
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->keyboard) {
	input->keyboard = wl_seat_get_keyboard(seat);

	wl_keyboard_add_listener(input->keyboard, &keyboard_listener, input);

    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {

	wl_keyboard_destroy(input->keyboard);
	input->keyboard = NULL;
    }
#endif
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
};

/**
 * new seat added
 */
static struct wld_input *
new_input_seat(struct wld_connection *connection,
	       struct wl_registry *registry,
	       uint32_t id)
{
    struct wld_input *input;

    input = calloc(1, sizeof(struct wld_input));
    if (input == NULL) {
	return NULL;
    }

    input->connection = connection;

    input->seat = wl_registry_bind(registry, id, &wl_seat_interface, 1);
    if (input->seat == NULL) {
	free(input);
	return NULL;
    }

    wl_seat_add_listener(input->seat, &seat_listener, input);

    return input;
}

/** registry global addition callback
 *
 * @param ctx context from when listener was added
 */
static void
registry_handle_global(void *ctx,
		       struct wl_registry *registry,
		       uint32_t id,
		       const char *interface,
		       uint32_t version)
{
    struct wld_connection* connection = ctx;

    /* process new interfaces appearing on the global registry */

    if (strcmp(interface, "wl_compositor") == 0) {
	/* compositor interface is available. Bind the interface */
	connection->compositor = wl_registry_bind(registry,
						  id,
						  &wl_compositor_interface,
						  1);

    } else if (strcmp(interface, "wl_shell") == 0) {
	/* shell interface is available. Bind the interface */
	connection->shell = wl_registry_bind(registry,
					     id,
					     &wl_shell_interface,
					     1);

    } else if (strcmp(interface, "wl_shm") == 0) {
	/* shared memory interface is available. Bind the interface
	 * and add a listener for the shared memory callbacks
	 */
	connection->shm = wl_registry_bind(registry,
					   id,
					   &wl_shm_interface,
					   1);
	if (connection->shm != NULL) {
	    connection->shm_formats = 0;
	    wl_shm_add_listener(connection->shm, &shm_listenter, connection);
	}

    } else if (strcmp(interface, "wl_seat") == 0) {
	struct wld_input *input;

	input = new_input_seat(connection, registry, id);
	if (input != NULL) {
	    wl_list_insert(connection->input_list.prev, &input->link);
	}
    }
}

/** registry global removal callback */
static void
registry_handle_global_remove(void *data,
			      struct wl_registry *registry,
			      uint32_t name)
{
}

/** registry global callback handlers */
static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};


static void
free_connection(struct wld_connection* connection)
{
    if (connection->compositor != NULL) {
	wl_compositor_destroy(connection->compositor);
    }

    if (connection->shell != NULL) {
	wl_shell_destroy(connection->shell);
    }

    if (connection->shm != NULL) {
	wl_shm_destroy(connection->shm);
    }

    wl_registry_destroy(connection->registry);

    wl_display_flush(connection->display);

    wl_display_disconnect(connection->display);

    free(connection);
}

/** construct a new connection to the wayland instance and aquire all
 * necessary global objects
 */
static struct wld_connection*
new_connection(void)
{
    struct wld_connection* connection;

    connection = calloc(1, sizeof(struct wld_connection));

    if (connection == NULL) {
	return NULL;
    }

    /* initialise lists */
    wl_list_init(&connection->input_list);

    /* make a connection to the wayland server */
    connection->display = wl_display_connect(NULL);
    if (connection->display == NULL) {
	fprintf(stderr, "Unable to open display\n");
	free(connection);
	return NULL;
    }

    /* create the registry object and attach its callback handlers */
    connection->registry = wl_display_get_registry(connection->display);
    if (connection->registry == NULL) {
	fprintf(stderr, "Unable create registry\n");
	wl_display_flush(connection->display);
	wl_display_disconnect(connection->display);
	free(connection);
	return NULL;
    }

    wl_registry_add_listener(connection->registry,
			     &registry_listener,
			     connection);
    /* perform a round trip to ensure registry messages have been processed */
    wl_display_roundtrip(connection->display);

    /* all global objects from the registry should now be available. */

    if (connection->shm == NULL) {
	/* The shared memory interface is available so fail. */
	fprintf(stderr, "No shared memory interface available\n");

	free_connection(connection);

	return NULL;
    }

    /* perform a round trip to ensure interface messages have been processed */
    wl_display_roundtrip(connection->display);

    /* check the XRGB8888 shared memory format is available */
    if (!(connection->shm_formats & (1 << WL_SHM_FORMAT_XRGB8888))) {
	fprintf(stderr, "WL_SHM_FORMAT_XRGB8888 not available\n");

	free_connection(connection);

	return NULL;
    }

    return connection;
}



static int
update_and_redraw(struct wldstate_s *wldstate,
		  int x,
		  int y,
		  int width,
		  int height)
{
    wl_surface_attach(wldstate->window->surface,
		      wldstate->shm_buffer->buffer,
		      0,
		      0);

    wl_surface_damage(wldstate->window->surface, x, y, width, height);

    wl_surface_commit(wldstate->window->surface);
    wldstate->shm_buffer->inuse = true;

    /* force syncronisation to cause the update */
    wl_display_roundtrip(wldstate->connection->display);

    return 0;
}

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
							uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
		 uint32_t edges, int32_t width, int32_t height)
{
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	handle_ping,
	handle_configure,
	handle_popup_done
};

static struct wld_window *
new_window(struct wld_connection *connection, int width, int height)
{
    struct wld_window *window;

    window = calloc(1, sizeof *window);
    if (window == NULL) {
	return NULL;
    }

    window->connection = connection;
    window->width = width;
    window->height = height;

    window->surface = wl_compositor_create_surface(connection->compositor);
    if (window->surface == NULL) {
	fprintf(stderr, "failed to create compositor surface\n");
	free(window);
	return NULL;
    }

    window->shell_surface = wl_shell_get_shell_surface(connection->shell,
						       window->surface);
    if (window->shell_surface == NULL) {
	fprintf(stderr, "failed to create shell surface\n");

	wl_surface_destroy(window->surface);

	free(window);
	return NULL;
    }

    wl_shell_surface_add_listener(window->shell_surface,
				  &shell_surface_listener, window);

    wl_shell_surface_set_title(window->shell_surface, "nsfb");

    wl_shell_surface_set_toplevel(window->shell_surface);

    return window;
}

static void
free_window(struct wld_window *window)
{

    wl_shell_surface_destroy(window->shell_surface);
    wl_surface_destroy(window->surface);

    free(window);
}

/*
 * Create a new, unique, anonymous file of the given size, and
 * return the file descriptor for it. The file descriptor is set
 * CLOEXEC. The file is immediately suitable for mmap()'ing
 * the given size at offset zero.
 *
 * The file should not have a permanent backing store like a disk,
 * but may have if XDG_RUNTIME_DIR is not properly implemented in OS.
 *
 * The file name is deleted from the file system.
 *
 * The file is suitable for buffer sharing between processes by
 * transmitting the file descriptor over Unix sockets using the
 * SCM_RIGHTS methods.
 */
static int
os_create_anonymous_file(off_t size)
{
	static const char template[] = "/weston-shared-XXXXXX";
	const char *path;
	char *name;
	int fd;

	path = getenv("XDG_RUNTIME_DIR");
	if (!path) {
		errno = ENOENT;
		return -1;
	}

	name = malloc(strlen(path) + sizeof(template));
	if (!name)
		return -1;

	strcpy(name, path);
	strcat(name, template);

	fd = mkostemp(name, O_CLOEXEC);
	if (fd >= 0)
		unlink(name);

	free(name);

	if (fd < 0)
		return -1;

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}


static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct wld_shm_buffer *shmbuf = data;

	shmbuf->inuse = false;
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static struct wld_shm_buffer *
new_shm_buffer(struct wl_shm *shm,
	       int width,
	       int height,
	       uint32_t format)
{
    struct wl_shm_pool *pool;
    struct wld_shm_buffer *shmbuff;
    int fd;
    int stride;

    shmbuff = calloc(1, sizeof(struct wld_shm_buffer));
    if (shmbuff == NULL) {
	return NULL;
    }

    stride = width * 4;
    shmbuff->size = stride * height;

    fd = os_create_anonymous_file(shmbuff->size);
    if (fd < 0) {
	free(shmbuff);
	return NULL;
    }

    shmbuff->data = mmap(NULL,
			 shmbuff->size,
			 PROT_READ | PROT_WRITE,
			 MAP_SHARED,
			 fd,
			 0);
    if (shmbuff->data == MAP_FAILED) {
	close(fd);
	free(shmbuff);
	return NULL;
    }

    pool = wl_shm_create_pool(shm, fd, shmbuff->size);
    shmbuff->buffer = wl_shm_pool_create_buffer(pool,
						0,
						width,
						height,
						stride,
						format);
    wl_shm_pool_destroy(pool);
    close(fd);

    if (shmbuff->buffer == NULL) {
	munmap(shmbuff->data, shmbuff->size);
	free(shmbuff);
	return NULL;
    }

    wl_buffer_add_listener(shmbuff->buffer, &buffer_listener, shmbuff);

    return shmbuff;
}

static void free_shm_buffer(struct wld_shm_buffer *shmbuf)
{
    munmap(shmbuf->data, shmbuf->size);
    free(shmbuf);
}

static int wld_initialise(nsfb_t *nsfb)
{
    wldstate_t *wldstate = nsfb->surface_priv;

    if (wldstate != NULL)
	return -1; /* already initialised */

    /* check bpp is what we can support. */
    if (nsfb->bpp != 32) {
	return -1;
    }

    wldstate = calloc(1, sizeof(wldstate_t));
    if (wldstate == NULL) {
	return -1; /* no memory */
    }

    wldstate->connection = new_connection();
    if (wldstate->connection == NULL) {
	fprintf(stderr, "Error initialising wayland connection\n");

	free(wldstate);

	return -1; /* error */
    }

    wldstate->window = new_window(wldstate->connection,
				  nsfb->width,
				  nsfb->height);
    if (wldstate->window == NULL) {
	fprintf(stderr, "Error creating wayland window\n");

	free_connection(wldstate->connection);

	free(wldstate);

	return -1; /* error */
    }

    wldstate->shm_buffer = new_shm_buffer(wldstate->connection->shm,
					  nsfb->width,
					  nsfb->height,
					  WL_SHM_FORMAT_XRGB8888);
    if (wldstate->shm_buffer == NULL) {
	fprintf(stderr, "Error creating wayland shared memory\n");

	free_window(wldstate->window);

	free_connection(wldstate->connection);

	free(wldstate);

	return -1; /* error */
    }

    nsfb->ptr = wldstate->shm_buffer->data;
    nsfb->linelen = nsfb->width * 4;

    update_and_redraw(wldstate,0,0, nsfb->width, nsfb->height);

    nsfb->surface_priv = wldstate;

    return 0;
}

static int wld_finalise(nsfb_t *nsfb)
{
    wldstate_t *wldstate = nsfb->surface_priv;

    if (wldstate == NULL) {
	return 0; /* not initialised */
    }

    free_shm_buffer(wldstate->shm_buffer);

    free_window(wldstate->window);

    free_connection(wldstate->connection);
}

#if 0
static int x_initialise(nsfb_t *nsfb)
{
    uint32_t mask;
    uint32_t values[3];
    xcb_size_hints_t *hints;
    xstate_t *xstate = nsfb->surface_priv;
    xcb_cursor_t blank_cursor;

    if (xstate != NULL)
	return -1; /* already initialised */

    /* sanity check bpp. */
    if ((nsfb->bpp != 32) && (nsfb->bpp != 16) && (nsfb->bpp != 8))
	return -1;

    xstate = calloc(1, sizeof(xstate_t));
    if (xstate == NULL)
	return -1; /* no memory */

    /* open connection with the server */
    xstate->connection = xcb_connect(NULL, NULL);
    if (xstate->connection == NULL) {
	fprintf(stderr, "Memory error opening display\n");
	free(xstate);
	return -1; /* no memory */
    }

    if (xcb_connection_has_error(xstate->connection) != 0) {
	fprintf(stderr, "Error opening display\n");
	free(xstate);
	return -1; /* no memory */
    }

    /* get screen */
    xstate->screen = xcb_setup_roots_iterator(xcb_get_setup(xstate->connection)).data;

    /* create image */
    xstate->image = create_shm_image(xstate, nsfb->width, nsfb->height, nsfb->bpp);

    if (xstate->image == NULL)
	xstate->image = create_image(xstate->connection, nsfb->width, nsfb->height, nsfb->bpp);

    if (xstate->image == NULL) {
	fprintf(stderr, "Unable to create image\n");
	free(xstate);
	xcb_disconnect(xstate->connection);
	return -1;
    }

    /* ensure plotting information is stored */
    nsfb->surface_priv = xstate;
    nsfb->ptr = xstate->image->data;
    nsfb->linelen = xstate->image->stride;

    /* get blank cursor */
    blank_cursor = create_blank_cursor(xstate->connection, xstate->screen);

    /* get keysymbol maps */
    xstate->keysymbols = xcb_key_symbols_alloc(xstate->connection);

    /* create window */
    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
    values[0] = xstate->screen->white_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_KEY_PRESS |
		XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE |
		XCB_EVENT_MASK_POINTER_MOTION;
    values[2] = blank_cursor;

    xstate->window = xcb_generate_id(xstate->connection);
    xcb_create_window (xstate->connection,
		       XCB_COPY_FROM_PARENT,
		       xstate->window,
		       xstate->screen->root,
		       0, 0, xstate->image->width, xstate->image->height, 1,
		       XCB_WINDOW_CLASS_INPUT_OUTPUT,
		       xstate->screen->root_visual,
		       mask, values);
    /* set size hits on window */
    hints = xcb_alloc_size_hints();
    xcb_size_hints_set_max_size(hints, xstate->image->width, xstate->image->height);
    xcb_size_hints_set_min_size(hints, xstate->image->width, xstate->image->height);
    xcb_set_wm_size_hints(xstate->connection, xstate->window, WM_NORMAL_HINTS, hints);
    xcb_free_size_hints(hints);

    /* create backing pixmap */
    xstate->pmap = xcb_generate_id(xstate->connection);
    xcb_create_pixmap(xstate->connection, 24, xstate->pmap, xstate->window, xstate->image->width, xstate->image->height);

    /* create pixmap plot gc */
    mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
    values[0] = xstate->screen->black_pixel;
    values[1] = 0xffffff;

    xstate->gc = xcb_generate_id (xstate->connection);
    xcb_create_gc(xstate->connection, xstate->gc, xstate->pmap, mask, values);

    /*    if (nsfb->bpp == 8)
	  set_palette(nsfb);
    */

    /* put the image into the pixmap */
    update_and_redraw_pixmap(xstate, 0, 0, xstate->image->width, xstate->image->height);


    /* show the window */
    xcb_map_window (xstate->connection, xstate->window);
    xcb_flush(xstate->connection);

    return 0;
}

static int x_finalise(nsfb_t *nsfb)
{
    xstate_t *xstate = nsfb->surface_priv;
    if (xstate == NULL)
	return 0;

    xcb_key_symbols_free(xstate->keysymbols);

    /* free pixmap */
    xcb_free_pixmap(xstate->connection, xstate->pmap);

    /* close connection to server */
    xcb_disconnect(xstate->connection);

    return 0;
}
#endif

static bool wld_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    wldstate_t *wldstate = nsfb->surface_priv;
    int ret = 0; /* number of events dispatched */
    struct wld_event* wldevent; /* input event off queue */

    if (wldstate == NULL) {
	return false;
    }

    /* flush any pending outgoing messages to server */
    wl_display_flush(wldstate->connection->display);

    /* if there are queued input events, return them first */
    wldevent = dequeue_wld_event(wldstate->connection);
    if (wldevent != NULL) {
	*event = wldevent->event;
	free(wldevent);
	return true;
    }

    if (timeout < 0) {
	/* caller wants to wait forever for an event */
	ret = wl_display_dispatch(wldstate->connection->display);
    } else {
	int confd;
	fd_set rfds;
	struct timeval tv;
	int retval;

	confd = wl_display_get_fd(wldstate->connection->display);

	FD_ZERO(&rfds);
	FD_SET(confd, &rfds);

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = timeout % 1000;

	retval = select(confd + 1, &rfds, NULL, NULL, &tv);
	if (retval == 0) {
	    /* timeout, nothing ready to read */
	    ret = wl_display_dispatch_pending(wldstate->connection->display);
	} else {
	    ret = wl_display_dispatch(wldstate->connection->display);
	}
    }

    /* check for connection error */
    if (ret == -1) {
	/* exit on conenction error */
	event->type = NSFB_EVENT_CONTROL;
	event->value.controlcode = NSFB_CONTROL_QUIT;
	return true;

    } else if (ret == 0) {
	/* timeout and no messages were processed and the input queue was
	 * empty on entry
	 */
	event->type = NSFB_EVENT_CONTROL;
	event->value.controlcode = NSFB_CONTROL_TIMEOUT;
	return true;
    }

    /* messages were processed, they might have been input events */

    wldevent = dequeue_wld_event(wldstate->connection);
    if (wldevent == NULL) {
	/* messages were not input events so signal no event */
	return false;
    }

    *event = wldevent->event;

    free(wldevent);

    return true;
}

#if 0
static bool x_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
    xcb_generic_event_t *e;
    xcb_expose_event_t *ee;
    xcb_motion_notify_event_t *emn;
    xcb_button_press_event_t *ebp;
    xcb_key_press_event_t *ekp;
    xcb_key_press_event_t *ekr;
    xstate_t *xstate = nsfb->surface_priv;

    if (xstate == NULL)
	return false;

    xcb_flush(xstate->connection);

    /* try and retrive an event immediately */
    e = xcb_poll_for_event(xstate->connection);

    if ((e == NULL) && (timeout != 0)) {
	if (timeout > 0) {
	    int confd;
	    fd_set rfds;
	    struct timeval tv;
	    int retval;

	    confd = xcb_get_file_descriptor(xstate->connection);
	    FD_ZERO(&rfds);
	    FD_SET(confd, &rfds);

	    tv.tv_sec = timeout / 1000;
	    tv.tv_usec = timeout % 1000;

	    retval = select(confd + 1, &rfds, NULL, NULL, &tv);
	    if (retval == 0) {
		    /* timeout, nothing happened */
		    event->type = NSFB_EVENT_CONTROL;
		    event->value.controlcode = NSFB_CONTROL_TIMEOUT;
		    return true;
	    }
	}
	e = xcb_wait_for_event(xstate->connection);
    }

    if (e == NULL) {
	if (xcb_connection_has_error(xstate->connection) != 0) {
	    /* connection closed quiting time */
	    event->type = NSFB_EVENT_CONTROL;
	    event->value.controlcode = NSFB_CONTROL_QUIT;
	    return true;
	} else {
	    return false; /* no event */
	}
    }

    event->type = NSFB_EVENT_NONE;

    switch (e->response_type) {
    case XCB_EXPOSE:
	ee = (xcb_expose_event_t *)e;
	xcb_copy_area(xstate->connection,
		      xstate->pmap,
		      xstate->window,
		      xstate->gc,
		      ee->x, ee->y,
		      ee->x, ee->y,
		      ee->width, ee->height);
	xcb_flush (xstate->connection);
	break;

    case XCB_MOTION_NOTIFY:
	emn = (xcb_motion_notify_event_t *)e;
	event->type = NSFB_EVENT_MOVE_ABSOLUTE;
	event->value.vector.x = emn->event_x;
	event->value.vector.y = emn->event_y;
	event->value.vector.z = 0;
	break;


    case XCB_BUTTON_PRESS:
	ebp = (xcb_button_press_event_t *)e;
	event->type = NSFB_EVENT_KEY_DOWN;

	switch (ebp->detail) {

	case X_BUTTON_LEFT:
	    event->value.keycode = NSFB_KEY_MOUSE_1;
	    break;

	case X_BUTTON_MIDDLE:
	    event->value.keycode = NSFB_KEY_MOUSE_2;
	    break;

	case X_BUTTON_RIGHT:
	    event->value.keycode = NSFB_KEY_MOUSE_3;
	    break;

	case X_BUTTON_WHEELUP:
	    event->value.keycode = NSFB_KEY_MOUSE_4;
	    break;

	case X_BUTTON_WHEELDOWN:
	    event->value.keycode = NSFB_KEY_MOUSE_5;
	    break;
	}
	break;

    case XCB_BUTTON_RELEASE:
	ebp = (xcb_button_press_event_t *)e;
	event->type = NSFB_EVENT_KEY_UP;

	switch (ebp->detail) {

	case X_BUTTON_LEFT:
	    event->value.keycode = NSFB_KEY_MOUSE_1;
	    break;

	case X_BUTTON_MIDDLE:
	    event->value.keycode = NSFB_KEY_MOUSE_2;
	    break;

	case X_BUTTON_RIGHT:
	    event->value.keycode = NSFB_KEY_MOUSE_3;
	    break;

	case X_BUTTON_WHEELUP:
	    event->value.keycode = NSFB_KEY_MOUSE_4;
	    break;

	case X_BUTTON_WHEELDOWN:
	    event->value.keycode = NSFB_KEY_MOUSE_5;
	    break;
	}
	break;


    case XCB_KEY_PRESS:
	ekp = (xcb_key_press_event_t *)e;
	event->type = NSFB_EVENT_KEY_DOWN;
	event->value.keycode = xkeysym_to_nsfbkeycode(xcb_key_symbols_get_keysym(xstate->keysymbols, ekp->detail, 0));
	break;

    case XCB_KEY_RELEASE:
	ekr = (xcb_key_release_event_t *)e;
	event->type = NSFB_EVENT_KEY_UP;
	event->value.keycode = xkeysym_to_nsfbkeycode(xcb_key_symbols_get_keysym(xstate->keysymbols, ekr->detail, 0));
	break;

    }

    free(e);

    return true;
}
#endif

static int wld_claim(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    struct nsfb_cursor_s *cursor = nsfb->cursor;

    if ((cursor != NULL) &&
	(cursor->plotted == true) &&
	(nsfb_plot_bbox_intersect(box, &cursor->loc))) {
	nsfb_cursor_clear(nsfb, cursor);
    }
    return 0;
}

static int
wld_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
    wldstate_t *wldstate = nsfb->surface_priv;
    nsfb_bbox_t redraw;
    nsfb_bbox_t fbarea;

    if ((cursor != NULL) && (cursor->plotted == true)) {

	nsfb_plot_add_rect(&cursor->savloc, &cursor->loc, &redraw);

	/* screen area */
	fbarea.x0 = 0;
	fbarea.y0 = 0;
	fbarea.x1 = nsfb->width;
	fbarea.y1 = nsfb->height;

	nsfb_plot_clip(&fbarea, &redraw);

	nsfb_cursor_clear(nsfb, cursor);

	nsfb_cursor_plot(nsfb, cursor);

	/* TODO: This is hediously ineficient - should keep the pointer image
	 * as a surface and composite server side
	 */
	update_and_redraw(wldstate, redraw.x0, redraw.y0, redraw.x1 - redraw.x0, redraw.y1 - redraw.y0);

    }
    return true;

}


static int wld_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    wldstate_t *wldstate = nsfb->surface_priv;
    struct nsfb_cursor_s *cursor = nsfb->cursor;

    if ((cursor != NULL) && (cursor->plotted == false)) {
	nsfb_cursor_plot(nsfb, cursor);
    }

    if (wldstate != NULL) {
	update_and_redraw(wldstate, box->x0, box->y0, box->x1 - box->x0, box->y1 - box->y0);
    }
    return 0;
}


const nsfb_surface_rtns_t wld_rtns = {
    .initialise = wld_initialise,
    .finalise = wld_finalise,
    .input = wld_input,
    .claim = wld_claim,
    .update = wld_update,
    .cursor = wld_cursor,
    .geometry = wld_set_geometry,
};

NSFB_SURFACE_DEF(wld, NSFB_SURFACE_WL, &wld_rtns)

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
