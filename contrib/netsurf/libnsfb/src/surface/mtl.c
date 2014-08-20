/*
 * Copyright (c) 2014 Stacey D. Son (sson@FreeBSD.org)
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/endian.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#include "libnsfb.h"
#include "libnsfb_event.h"
#include "libnsfb_plot.h"
#include "libnsfb_plot_util.h"

#include "nsfb.h"
#include "plot.h"
#include "surface.h"
#include "cursor.h"

#define	UNUSED(x)	((x) = (x))

#define MTL_REG0	"/dev/mtl_reg0"
#define	MTL_PIXEL0	"/dev/mtl_pixel0"

/* Integer offsets of the MTL registers. */
#define	MTL_REG_X1				3
#define	MTL_REG_Y1				4
#define	MTL_REG_X2				5
#define	MTL_REG_Y2				6
#define	MTL_REG_COUNT_GESTURE	7

typedef enum touch_event {
	TE_NONE	=	0,
	TE_MOVE,
	TE_PENDOWN,
	TE_PENUP
} touch_event_t;

/*
 * Touch Gesture Events.
 *
 * The following is derived from the "Terasic Multi-Touch LCD Users Manual"
 * (Table 3-4, Pg 16-17).
 */
typedef enum gestures {
	TGE_NONE = 		0x00,	/* No gesture. */

	/* One Point Gestures: */
	TGE_1P_NORTH =		0x10,	/* Drag Single North. */
	TGE_1P_NORTHEAST =	0x12,	/* Drag Single NorthEast. */
	TGE_1P_EAST =		0x14,	/* Drag Single East. */
	TGE_1P_SOUTHEAST = 	0x16,	/* Drag Single SouthEast. */
	TGE_1P_SOUTH =		0x18,	/* Drag Single South. */
	TGE_1P_SOUTHWEST =	0x1A,	/* Drag Single SouthWest. */
	TGE_1P_WEST =		0x1C,	/* Drag Single West. */
	TGE_1P_NORTHWEST = 	0x1E,	/* Drag Single NorthWest. */
	TGE_1P_ROTATE_CW =	0x28,	/* Drag Single in Clockwise Motion. */
	TGE_1P_ROTATE_CCW =	0x29,	/* Drag Single in Counter Clockwise */
	TGE_1P_CLICK =		0x20,	/* One Tap with Single. */
	TGE_1P_DCLICK = 	0x22,	/* Double Tap with Single. */

	/* Two Point Gestures: */
	TGE_2P_NORTH =		0x30,	/* Drag Double North. */
	TGE_2P_NORTHEAST =	0x32,	/* Drag Double NorthEast. */
	TGE_2P_EAST =		0x34,	/* Drag Double East. */
	TGE_2P_SOUTHEAST = 	0x36,	/* Drag Double SouthEast. */
	TGE_2P_SOUTH =		0x38,	/* Drag Double South. */
	TGE_2P_SOUTHWEST =	0x3A,	/* Drag Double SouthWest. */
	TGE_2P_WEST =		0x3C,	/* Drag Double West. */
	TGE_2P_NORTHWEST =	0x3E,	/* Drag Double NorthWest. */
	TGE_2P_CLICK =		0x40,	/* Tap Once with Double. */
	TGE_2P_ZOOM_IN =	0x48,	/* Pinch Outward with Double. */
	TGE_2P_ZOOM_OUT = 	0x49	/* Pinch Inward with Double. */
} gesture_t;

typedef struct {
        int 		ts_x1;	/* X coordinate of 1st touch point. */
        int 		ts_y1;	/* Y coordinate of 1st touch point. */
        int 		ts_x2;	/* X coordinate of 2nd touch point. */
        int 		ts_y2;	/* Y coordinate of 2nd touch point. */
        int 		ts_count;	/* Number of points (0, 1, or, 2) */
        gesture_t	ts_gesture;	/* Gesture ID (see above). */
} touch_state_t;

struct mtl_priv {
	u_int32_t *mtlctrl;
	int ctrlfd;
	int dispfd;
	touch_state_t ts;
	touch_state_t rel_s;
	int check_release;
};

static inline int
get_mtl_reg_x1(struct mtl_priv *mtl)
{

	return (le32toh(mtl->mtlctrl[MTL_REG_X1]));
}

static inline int
get_mtl_reg_y1(struct mtl_priv *mtl)
{

	return (le32toh(mtl->mtlctrl[MTL_REG_Y1]));
}

static inline int
get_mtl_reg_x2(struct mtl_priv *mtl)
{

 	return (le32toh(mtl->mtlctrl[MTL_REG_X2]));
}

static inline int
get_mtl_reg_y2(struct mtl_priv *mtl)
{

	return (le32toh(mtl->mtlctrl[MTL_REG_Y2]));
}

static inline int
get_mtl_reg_cnt_gest(struct mtl_priv *mtl, int *count, gesture_t *gesture)
{
	int cnt_gest;

	cnt_gest = le32toh(mtl->mtlctrl[MTL_REG_COUNT_GESTURE]);
	if (cnt_gest < 0) {
		*count = 0;
		*gesture = TGE_NONE;
		return (-1);
	} else {
		*count = cnt_gest >> 8;
		*gesture = cnt_gest & 0xff;
		return (0);
	}
}

static inline int
get_touch_state(struct mtl_priv *mtl, touch_state_t *state)
{
	state->ts_x1 = get_mtl_reg_x1(mtl);
	state->ts_y1 = get_mtl_reg_y1(mtl);
	state->ts_x2 = get_mtl_reg_x2(mtl);
	state->ts_y2 = get_mtl_reg_y2(mtl);

	return (get_mtl_reg_cnt_gest(mtl, &state->ts_count,
			&state->ts_gesture));
}

#ifdef MTLDEBUG
static const char *
gesture_str(gesture_t gest)
{
    switch (gest) {
    case TGE_NONE:  return ("NONE");

    case TGE_1P_NORTH: return ("NORTH");
    case TGE_1P_NORTHEAST: return ("NORTHEAST");
    case TGE_1P_EAST: return ("EAST");
    case TGE_1P_SOUTHEAST: return ("SOUTHEAST");
    case TGE_1P_SOUTH: return ("SOUTH");
    case TGE_1P_SOUTHWEST: return ("SOUTHWEST");
    case TGE_1P_WEST: return ("WEST");
    case TGE_1P_NORTHWEST: return ("NORTHWEST");
    case TGE_1P_ROTATE_CW: return ("ROTATE_CW");
    case TGE_1P_ROTATE_CCW: return ("ROTATE_CCW");
    case TGE_1P_CLICK: return ("CLICK");
    case TGE_1P_DCLICK: return ("DCLICK");

    case TGE_2P_NORTH: return ("NORTH2");
    case TGE_2P_NORTHEAST: return ("NORTHEAST2");
    case TGE_2P_EAST: return ("EAST2");
    case TGE_2P_SOUTHEAST: return ("SOUTHEAST2");
    case TGE_2P_SOUTH: return ("SOUTH2");
    case TGE_2P_SOUTHWEST: return ("SOUTHWEST");
    case TGE_2P_WEST: return ("WEST2");
    case TGE_2P_NORTHWEST: return ("NORTHWEST2");
    case TGE_2P_CLICK: return ("CLICK2");
    case TGE_2P_ZOOM_IN: return ("ZOOM_IN2");
    case TGE_2P_ZOOM_OUT: return ("ZOOM_OUT2");

    default: return ("UNKNOWN");
    }
}
#endif

static touch_event_t
ts_poll(struct mtl_priv *mtl, int timeout)
{
        struct timespec stime = {0, 1000000}; /* 1 millisecond */
		int first_pass = 1;
        touch_state_t tmp_s;
		touch_state_t *rel_s = &mtl->rel_s;
		touch_state_t *sp = &mtl->ts;
        int loops = 0;

#if MTLDEBUG
printf("ts_poll(): timeout = %d check_release = %d\n", timeout, mtl->check_release);
#endif
if (timeout == 0)
		timeout = 1;

        for (;;) {
                if (timeout != 0 && !mtl->check_release && loops > timeout) {
                        /*
                         * If we have timed out and aren't waiting for a
                         * release then return an empty gesture.
                         */
                        sp->ts_count = 0;
                        sp->ts_gesture = TGE_NONE;
                        // return (sp);
						return TE_NONE;
                }
                if (get_touch_state(mtl, &tmp_s) < 0) {
                        if (mtl->check_release) {
#if MTLDEBUG
if (rel_s->ts_gesture == TGE_1P_CLICK || rel_s->ts_gesture == TGE_1P_DCLICK ||
	rel_s->ts_gesture == TGE_2P_CLICK) {
	printf("PEN UP: %s (%d)\n",

		gesture_str(rel_s->ts_gesture), rel_s->ts_count);
}
#endif
                                mtl->check_release = 0;
                                *sp = *rel_s;
                                // return (sp);
								return (TE_PENUP);
                        }
                        if (first_pass && sp->ts_count > 0) {
                                /*
                                 * If we returned a touch last time around
                                 * then fake up a release now.
                                 * XXX: we should probably have a timelimit
                                 */
                                sp->ts_count = 0;
                                sp->ts_gesture = TGE_NONE;
                                // return(sp);
								return (TE_NONE);
                        }
                        first_pass = 0;
                        nanosleep(&stime, NULL);
                        loops++;
                        continue;
                }

               if (
                    tmp_s.ts_x1 != sp->ts_x1 || tmp_s.ts_y1 != sp->ts_y1 ||
                    tmp_s.ts_x2 != sp->ts_x2 || tmp_s.ts_y2 != sp->ts_y2 ||
                    tmp_s.ts_count != sp->ts_count ||
                    tmp_s.ts_gesture != sp->ts_gesture) {
                        /*
                         * If we get an release event, differ returning
                         * it until we sleep and get a non-event.
                         */
                        if (tmp_s.ts_count == 0) {
                                mtl->check_release = 1;
                                mtl->rel_s = tmp_s;
#ifdef MTLDEBUG
if (mtl->rel_s.ts_gesture == TGE_1P_CLICK ||
mtl->rel_s.ts_gesture == TGE_1P_DCLICK ||
mtl->rel_s.ts_gesture == TGE_2P_CLICK) {
	printf("PEN DOWN (COUNT = 0): %s\n",
		gesture_str(mtl->rel_s.ts_gesture));
				return (TE_PENDOWN);
} else {
	printf("MOVE (COUNT = 0) ");
	printf("to (%d, %d) (%d, %d) %s\n", tmp_s.ts_x1, tmp_s.ts_y1, tmp_s.ts_x2, tmp_s.ts_y2, gesture_str(mtl->rel_s.ts_gesture));
}
#endif
                        } else {
				if (tmp_s.ts_x1 != -1 && tmp_s.ts_y1 != -1) {
        	                       	*sp = tmp_s;
#ifdef MTLDEBUG
printf("MOVE (COUNT = %d) (%d, %d) (%d, %d) ", tmp_s.ts_count, tmp_s.ts_x1, tmp_s.ts_y1, tmp_s.ts_x2, tmp_s.ts_y2);
printf("%s\n", gesture_str(tmp_s.ts_gesture));
#endif
					return (TE_MOVE);
				}
                        }
                }
                first_pass = 0;
                nanosleep(&stime, NULL);
                loops++;
        }
}

#ifdef MTLDEBUG
static const char *
touch_event_str(touch_event_t te)
{

	switch(te) {
	case TE_NONE: return ("NONE");
	case TE_MOVE: return ("MOVE");
	case TE_PENUP: return ("CLICK UP");
	case TE_PENDOWN: return ("CLICK DOWN");
	default: return ("UNKNOWN");
	}
}
#endif

#if 0
static void
ts_drain(struct mtl_priv *mtl)
{
        struct timespec stime = {0, 1000000};
        int noprevtouch = 0;
        int cnt;
        gesture_t gest;
	touch_state_t *sp = &mtl->ts;

        if (sp == NULL || sp->ts_count == 0)
                noprevtouch = 1;

        for (;;) {
                nanosleep(&stime, NULL);
                if (get_mtl_reg_cnt_gest(mtl, &cnt, &gest) < 0) {
                        if (noprevtouch)
                                return;
                        else
                                noprevtouch++;
                }
        }
}
#endif

static int
mtl_defaults(nsfb_t *nsfb)
{

	nsfb->width = 800;
	nsfb->height = 480;
	nsfb->format = NSFB_FMT_ARGB8888;

	/* select default sw plotters for bpp */
	select_plotters(nsfb);

	return (0);
}

static int
mtl_initialise(nsfb_t *nsfb)
{
	size_t size;
	struct mtl_priv *mtl;

	mtl = calloc(1, sizeof(struct mtl_priv));
	if (mtl == NULL) {
		printf("Unable to malloc mtl_priv\n");
		return (-1);
	}
	nsfb->linelen = (nsfb->width * nsfb->bpp) / 8;
	nsfb->width = 800;
	nsfb->height = 480;

	size = (nsfb->width * nsfb->height * nsfb->bpp) / 8;

	mtl->ctrlfd = open(MTL_REG0, O_RDWR | O_NONBLOCK);
    	if (mtl->ctrlfd < 0) {
		printf("Unable to open %s\n", MTL_REG0);
		free(mtl);
		return (-1);
    	}
	mtl->mtlctrl = mmap(NULL, 0x20, PROT_READ | PROT_WRITE, MAP_SHARED,
					mtl->ctrlfd, 0);
    	if (mtl->mtlctrl == MAP_FAILED) {
		printf("Unable to mmap %s\n", MTL_REG0);
		free(mtl);
		return (-1);
    	}
	/* XXX Need to set blending */

    	mtl->dispfd = open(MTL_PIXEL0, O_RDWR | O_NONBLOCK);
    	if (mtl->dispfd < 0) {
		printf("Unable to open %s\n", MTL_PIXEL0);
		free(mtl);
		return (-1);
    	}
    	nsfb->ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
					mtl->dispfd, 0);
    	if (nsfb->ptr == MAP_FAILED) {
		printf("Unable to mmap %s\n", MTL_PIXEL0);
		free(mtl);
		return (-1);
    	}
	memset(nsfb->ptr, 0xff, size);

    	nsfb->surface_priv = mtl;

    	return (0);
}

static int
mtl_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format)
{
	UNUSED(width);
	UNUSED(height);
	UNUSED(format);

	/* We only support one geometry. */
	nsfb->width = 800;
	nsfb->height = 480;
	nsfb->format = NSFB_FMT_ABGR8888;

	/* select soft plotters appropriate for format */
	select_plotters(nsfb);

	return (0);
}


static int
mtl_finalise(nsfb_t *nsfb)
{
	struct mtl_priv *mtl = nsfb->surface_priv;

	if (mtl != NULL) {
		munmap(nsfb->ptr, 0);
		close(mtl->dispfd);
		munmap(mtl->mtlctrl, 0);
		close(mtl->ctrlfd);
		free(mtl);
		mtl = NULL;
	}
	return (0);
}

static bool mtl_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{

	struct mtl_priv *mtl = nsfb->surface_priv;

	switch(ts_poll(mtl, timeout)) {
	case TE_NONE:
			event->type = NSFB_EVENT_NONE;
			break;

	case TE_MOVE:
			event->type = NSFB_EVENT_MOVE_ABSOLUTE;
			event->value.vector.x = mtl->ts.ts_x1;
			event->value.vector.y = mtl->ts.ts_y1;
			event->value.vector.z = 0;
			break;

	case TE_PENDOWN:
			event->type = NSFB_EVENT_KEY_DOWN;
			event->value.keycode = NSFB_KEY_MOUSE_1;
			break;

	case TE_PENUP:
			event->type = NSFB_EVENT_KEY_UP;
			event->value.keycode = NSFB_KEY_MOUSE_1;
			break;

	default:
			event->type = NSFB_EVENT_NONE;
			return (false);
	}
	return (true);
}

static int mtl_claim(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    struct nsfb_cursor_s *cursor = nsfb->cursor;

    if ((cursor != NULL) &&
        (cursor->plotted == true) &&
        (nsfb_plot_bbox_intersect(box, &cursor->loc))) {

        nsfb->plotter_fns->bitmap(nsfb,
                                  &cursor->savloc,
                                  cursor->sav,
                                  cursor->sav_width,
                                  cursor->sav_height,
                                  cursor->sav_width,
                                  false);
        cursor->plotted = false;
    }
    return 0;
}

static int mtl_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
    nsfb_bbox_t sclip;

    if ((cursor != NULL) && (cursor->plotted == true)) {
        sclip = nsfb->clip;

        nsfb->plotter_fns->set_clip(nsfb, NULL);

        nsfb->plotter_fns->bitmap(nsfb,
                                  &cursor->savloc,
                                  cursor->sav,
                                  cursor->sav_width,
                                  cursor->sav_height,
                                  cursor->sav_width,
                                  false);

        nsfb_cursor_plot(nsfb, cursor);

        nsfb->clip = sclip;
    }
    return true;
}


static int mtl_update(nsfb_t *nsfb, nsfb_bbox_t *box)
{
    struct nsfb_cursor_s *cursor = nsfb->cursor;

    UNUSED(box);

    if ((cursor != NULL) && (cursor->plotted == false)) {
        nsfb_cursor_plot(nsfb, cursor);
    }

    return 0;
}

const nsfb_surface_rtns_t mtl_rtns = {
    .defaults = mtl_defaults,
    .initialise = mtl_initialise,
    .finalise = mtl_finalise,
    .input = mtl_input,
    .claim = mtl_claim,
    .update = mtl_update,
    .cursor = mtl_cursor,
    .geometry = mtl_set_geometry,
};

NSFB_SURFACE_DEF(mtl, NSFB_SURFACE_MTL, &mtl_rtns)

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
