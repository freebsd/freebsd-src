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
#include <sys/select.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <termios.h>
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

#define	MTL_RES_WIDTH	800
#define	MTL_RES_HEIGHT	480
#define	MTL_FORMAT	NSFB_FMT_XRGB8888

/* Integer offsets of the MTL registers. */
#define	MTL_REG_X1				3
#define	MTL_REG_Y1				4
#define	MTL_REG_X2				5
#define	MTL_REG_Y2				6
#define	MTL_REG_COUNT_GESTURE	7

#ifdef MTL_TOUCHINPUT
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
	int mousefd;
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

static bool
mtl_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
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

#else /* ! MTL_TOUCHINPUT */


#include <sys/mouse.h>
#include <sys/consio.h>

enum nsfb_key_code_e mtl_nsfb_map1[] = {

    NSFB_KEY_UNKNOWN,		/* 00 */
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_BACKSPACE,
    NSFB_KEY_TAB,
    NSFB_KEY_RETURN,		/* 0A */
    NSFB_KEY_CLEAR,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_RETURN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,		/* 0F */

    NSFB_KEY_UNKNOWN,		/* 10 */
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_ESCAPE,		/* 1B */
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,
    NSFB_KEY_UNKNOWN,

    NSFB_KEY_SPACE,		/* 20 */
    NSFB_KEY_EXCLAIM,
    NSFB_KEY_QUOTEDBL,
    NSFB_KEY_HASH,
    NSFB_KEY_DOLLAR,
    NSFB_KEY_5,			/* % */
    NSFB_KEY_AMPERSAND,
    NSFB_KEY_QUOTE,
    NSFB_KEY_LEFTPAREN,
    NSFB_KEY_RIGHTPAREN,
    NSFB_KEY_ASTERISK,
    NSFB_KEY_PLUS,
    NSFB_KEY_COMMA,
    NSFB_KEY_MINUS,
    NSFB_KEY_PERIOD,
    NSFB_KEY_SLASH,
    NSFB_KEY_0,
    NSFB_KEY_1,
    NSFB_KEY_2,
    NSFB_KEY_3,
    NSFB_KEY_4,
    NSFB_KEY_5,
    NSFB_KEY_6,
    NSFB_KEY_7,
    NSFB_KEY_8,
    NSFB_KEY_9,
    NSFB_KEY_COLON,
    NSFB_KEY_SEMICOLON,
    NSFB_KEY_LESS,
    NSFB_KEY_EQUALS,
    NSFB_KEY_GREATER,
    NSFB_KEY_QUESTION,
    NSFB_KEY_AT,		/* 40 */

    NSFB_KEY_a,			/* 41 */
    NSFB_KEY_b,
    NSFB_KEY_c,
    NSFB_KEY_d,
    NSFB_KEY_e,
    NSFB_KEY_f,
    NSFB_KEY_g,
    NSFB_KEY_h,
    NSFB_KEY_i,
    NSFB_KEY_j,
    NSFB_KEY_k,
    NSFB_KEY_l,
    NSFB_KEY_m,
    NSFB_KEY_n,
    NSFB_KEY_o,
    NSFB_KEY_p,
    NSFB_KEY_q,
    NSFB_KEY_r,
    NSFB_KEY_s,
    NSFB_KEY_t,
    NSFB_KEY_u,
    NSFB_KEY_v,
    NSFB_KEY_w,
    NSFB_KEY_x,
    NSFB_KEY_y,
    NSFB_KEY_z,			/* 5A */

    NSFB_KEY_LEFTBRACKET,	/* 5B */
    NSFB_KEY_BACKSLASH,
    NSFB_KEY_RIGHTBRACKET,
    NSFB_KEY_CARET,
    NSFB_KEY_UNDERSCORE,
    NSFB_KEY_BACKQUOTE,		/* 60 */

    NSFB_KEY_a,			/* 61 */
    NSFB_KEY_b,
    NSFB_KEY_c,
    NSFB_KEY_d,
    NSFB_KEY_e,
    NSFB_KEY_f,
    NSFB_KEY_g,
    NSFB_KEY_h,
    NSFB_KEY_i,
    NSFB_KEY_j,
    NSFB_KEY_k,
    NSFB_KEY_l,
    NSFB_KEY_m,
    NSFB_KEY_n,
    NSFB_KEY_o,
    NSFB_KEY_p,
    NSFB_KEY_q,
    NSFB_KEY_r,
    NSFB_KEY_s,
    NSFB_KEY_t,
    NSFB_KEY_u,
    NSFB_KEY_v,
    NSFB_KEY_w,
    NSFB_KEY_x,
    NSFB_KEY_y,
    NSFB_KEY_z,			/* 7A */

    NSFB_KEY_LEFTBRACKET,		/* 7B */
    NSFB_KEY_BACKSLASH,
    NSFB_KEY_RIGHTBRACKET,
    NSFB_KEY_CARET,
    NSFB_KEY_DELETE,		/* 7F */
};


struct mtl_priv {
	u_int32_t *mtlctrl;

	/* I/O file descriptors */
	int ctrlfd;		/* MTL control */
	int dispfd;		/* display framebuffer */
	int mousefd;		/* mouse */
	int kbfd;		/* keyboard */

	/* Mouse state */
	int bright;		/* Mouse buttons */
	int bmiddle;
	int bleft;
	int abs_x;		/* Absolute mouse position */
	int abs_y;

	/* Keyboard state  */
	int keycode;		/* Keycode for pending event */
	int needup;		/* Key needs up event */
	int shiftdown;		/* Shift key is down */
	int shiftup;		/* Shift key needs up event */

	struct termios termios;
};


#define MTL_MOUSEDEV "/dev/ums0"	/* Use /dev/sysmouse if using moused. */

static bool
mtl_input(nsfb_t *nsfb, nsfb_event_t *event, int timeout)
{
	struct mtl_priv *mtl = nsfb->surface_priv;
	fd_set rfds;
	struct timeval tv;
	int to = timeout;
	int rsel, x, y, maxfd, numkeys;
	char me[8];
	unsigned char keys[20];

	/* First, see if we have pending events. */
	if (mtl->needup) {
		mtl->needup = 0;
		event->type = NSFB_EVENT_KEY_UP;
		event->value.keycode = mtl->keycode;
		return (true);
	}

	if (mtl->shiftdown) {
		mtl->shiftdown = 0;
		mtl->shiftup = mtl->needup = 1;
		event->type = NSFB_EVENT_KEY_DOWN;
		event->value.keycode = mtl->keycode;
		return (true);
	}

	if (mtl->shiftup) {
		mtl->shiftup = 0;
		event->type = NSFB_EVENT_KEY_UP;
		event->value.keycode = NSFB_KEY_RSHIFT;
		return (true);
	}

	FD_ZERO(&rfds);
	FD_SET(mtl->mousefd, &rfds);
	FD_SET(mtl->kbfd, &rfds);
	maxfd = (mtl->kbfd > mtl->mousefd) ? mtl->kbfd : mtl->mousefd;

	if (to > 10)			/* Need to add polling */
		to = 10;

	tv.tv_sec = to / 1000;
	tv.tv_usec = to % 1000;

	rsel = select(maxfd + 1, &rfds, NULL, NULL, &tv);
	if (rsel == 0) {
		/* timeout, nothing happened */
		event->type = NSFB_EVENT_CONTROL;
		event->value.controlcode = NSFB_CONTROL_TIMEOUT;

		return (true);
	}

	/* Check for keyboard event. */
	if (FD_ISSET(mtl->kbfd, &rfds)) {
		if ((numkeys = read(mtl->kbfd, &keys[0], sizeof(keys)))) {
			if (numkeys == 1 && keys[0] < 128) {
				if ((keys[0] >= 'A' && keys[0] <= 'Z') ||
				    (keys[0] >= '{' && keys[0] <= '}') ||
				    (keys[0] == '%')) {
					/*
					 * An upper-case or other key that
					 * requires shift.
					 */
					mtl->shiftdown = 1;
					mtl->keycode = mtl_nsfb_map1[keys[0]];
					event->type = NSFB_EVENT_KEY_DOWN;
					event->value.keycode = NSFB_KEY_RSHIFT;
				} else {
					mtl->needup = 1;
					event->type = NSFB_EVENT_KEY_DOWN;
					mtl->keycode = event->value.keycode =
						mtl_nsfb_map1[keys[0]];
				}
				return (true);
			}
		}
	}

	/* Check for mouse event. */
	if (!FD_ISSET(mtl->mousefd, &rfds))
		return (false);

	if (read(mtl->mousefd, &me[0], sizeof(me))) {
		char button = me[0] & 0x7F;

		/* Right mouse button events */
		if (mtl->bright && ((button & 0x1) == 0)) {
			mtl->bright = 0;
			event->type = NSFB_EVENT_KEY_DOWN;
			event->value.keycode = NSFB_KEY_MOUSE_3;
			return (true);
		}
		if (!mtl->bright && ((button & 0x1) != 0)) {
			mtl->bright = 1;
			event->type = NSFB_EVENT_KEY_UP;
			event->value.keycode = NSFB_KEY_MOUSE_3;
			return (true);
		}

		/* Left mouse button events */
		if (mtl->bleft && ((button & 0x4) == 0)) {
			mtl->bleft = 0;
			event->type = NSFB_EVENT_KEY_DOWN;
			event->value.keycode = NSFB_KEY_MOUSE_1;
			return (true);
		}
		if (!mtl->bleft && ((button & 0x4) != 0)) {
			mtl->bleft = 1;
			event->type = NSFB_EVENT_KEY_UP;
			event->value.keycode = NSFB_KEY_MOUSE_1;
			return (true);
		}

		/* Middle mouse button events */
		if (mtl->bmiddle && ((button & 0x2) == 0)) {
			mtl->bmiddle = 0;
			event->type = NSFB_EVENT_KEY_DOWN;
			event->value.keycode = NSFB_KEY_MOUSE_2;
			return (true);
		}
		if (!mtl->bmiddle && ((button & 0x2) != 0)) {
			mtl->bmiddle = 1;
			event->type = NSFB_EVENT_KEY_UP;
			event->value.keycode = NSFB_KEY_MOUSE_2;
			return (true);
		}

		/* Wheel up */
		if (me[5] != 0) {
			mtl->needup = 1;
			event->type = NSFB_EVENT_KEY_DOWN;
			mtl->keycode = event->value.keycode = NSFB_KEY_MOUSE_4;
			return (true);
		}

		/* Wheel Down */
		if (me[6] != 0) {
			mtl->needup = 1;
			event->type = NSFB_EVENT_KEY_DOWN;
			mtl->keycode = event->value.keycode = NSFB_KEY_MOUSE_5;
			return (true);
		}

		/* Calculate absolute x and y, limit to screen limits. */
		x = (int)me[1] + (int)me[3];
		y = (int)me[2] + (int)me[4];

		/* Update absolute x, y */
		mtl->abs_x += x;
		if (mtl->abs_x < 0)
			mtl->abs_x = 0;
		if (mtl->abs_x >= MTL_RES_WIDTH)
			mtl->abs_x = MTL_RES_WIDTH - 1;

		mtl->abs_y -= y;
		if (mtl->abs_y < 0)
			mtl->abs_y = 0;
		if (mtl->abs_y >= MTL_RES_HEIGHT)
			mtl->abs_y = MTL_RES_HEIGHT - 1;

		event->type = NSFB_EVENT_MOVE_ABSOLUTE;
		event->value.vector.x = mtl->abs_x;
		event->value.vector.y = mtl->abs_y;
		event->value.vector.z = 0;
		return (true);
	}

	return (false);
}
#endif /* ! MTL_TOUCHINPUT */

static int
mtl_defaults(nsfb_t *nsfb)
{

	nsfb->width = MTL_RES_WIDTH;
	nsfb->height = MTL_RES_HEIGHT;
	nsfb->format = MTL_FORMAT;

	/* select default sw plotters for bpp */
	select_plotters(nsfb);

	return (0);
}

static int
mtl_finalise(nsfb_t *nsfb)
{
	struct mtl_priv *mtl = nsfb->surface_priv;

	if (mtl != NULL) {
		memset(nsfb->ptr, 0x00,
			(nsfb->width * nsfb->height * nsfb->bpp) / 8);
		if (nsfb->ptr != NULL)
			munmap(nsfb->ptr, 0);
		if (mtl->dispfd > 0)
			close(mtl->dispfd);
		if (mtl->mtlctrl != NULL) {
			/* Set back to Big Endian */
			mtl->mtlctrl[0] = mtl->mtlctrl[0] | 0x10;
			munmap(mtl->mtlctrl, 0);
		}
		if (mtl->ctrlfd > 0)
			close(mtl->ctrlfd);
		if (mtl->mousefd > 0)
			close(mtl->mousefd);
		if (mtl->kbfd > 0)
			close(mtl->kbfd);
		free(mtl);
		mtl = NULL;
	}
	return (0);
}

static int
mtl_initialise(nsfb_t *nsfb)
{
	size_t size;
	struct mtl_priv *mtl;
	struct termios tios;

	mtl = calloc(1, sizeof(struct mtl_priv));
	if (mtl == NULL) {
		perror("malloc");
		return (-1);
	}
	nsfb->surface_priv = mtl;
	nsfb->linelen = (nsfb->width * nsfb->bpp) / 8;
	nsfb->width = MTL_RES_WIDTH;
	mtl->abs_x = nsfb->width / 2;
	nsfb->height = MTL_RES_HEIGHT;
	mtl->abs_y = nsfb->width / 2;
	nsfb->format = MTL_FORMAT;

	size = (nsfb->width * nsfb->height * nsfb->bpp) / 8;

	mtl->ctrlfd = open(MTL_REG0, O_RDWR | O_NONBLOCK);
    	if (mtl->ctrlfd < 0) {
		printf("Unable to open %s: %s\n", MTL_REG0, strerror(errno));
		(void)mtl_finalise(nsfb);
		return (-1);
    	}
	mtl->mtlctrl = mmap(NULL, 0x20, PROT_READ | PROT_WRITE, MAP_SHARED,
					mtl->ctrlfd, 0);
    	if (mtl->mtlctrl == MAP_FAILED) {
		printf("Unable to mmap %s: %s\n", MTL_REG0, strerror(errno));
		(void)mtl_finalise(nsfb);
		return (-1);
    	}

	/* Set blending and turn off TERASIC_MTL_BLEND_PIXEL_ENDIAN_SWAP */
	mtl->mtlctrl[0] = (0xff << 8) | (mtl->mtlctrl[0] & 0xef);

    	mtl->dispfd = open(MTL_PIXEL0, O_RDWR | O_NONBLOCK);
    	if (mtl->dispfd < 0) {
		printf("Unable to open %s: %s\n", MTL_PIXEL0, strerror(errno));
		(void)mtl_finalise(nsfb);
		return (-1);
    	}
    	nsfb->ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
					mtl->dispfd, 0);
    	if (nsfb->ptr == MAP_FAILED) {
		printf("Unable to mmap %s: %s\n", MTL_PIXEL0, strerror(errno));
		(void)mtl_finalise(nsfb);
		return (-1);
    	}
	memset(nsfb->ptr, 0xff, size);

	mtl->mousefd = open(MTL_MOUSEDEV, O_RDONLY);
	if (mtl->mousefd < 0) {
		printf("Unable to open %s: %s\n", MTL_MOUSEDEV,
			strerror(errno));
		(void)mtl_finalise(nsfb);
		return (-1);
	}
	int level = 1;
	ioctl(mtl->mousefd, MOUSE_SETLEVEL, &level);

	mtl->kbfd = open("/dev/ttyv0", O_RDONLY);
	if (mtl->kbfd < 0) {
		printf("Unable to open /dev/ttyv0: %s\n", strerror(errno));
		(void)mtl_finalise(nsfb);
		return (-1);
	}

	if (tcgetattr(mtl->kbfd, &mtl->termios) < 0) {
		perror("tcgetattr");
		(void)mtl_finalise(nsfb);
		return (-1);
	}

	tios = mtl->termios;
	tios.c_lflag &= ~ICANON;
	tios.c_lflag &= ~ECHO;
	tios.c_cc[VMIN] = 1;
	tios.c_cc[VTIME] = 0;
	if (tcsetattr(mtl->kbfd, TCSADRAIN, &tios) < 0) {
		perror("tcsetattr");
		(void)mtl_finalise(nsfb);
		return (-1);
	}

    	return (0);
}

static inline void
pixelmove(uint8_t *dst, uint8_t *src, size_t len, int bpp)
{
	if (bpp < 24) {
		memmove(dst, src, len);
	} else {
		uint32_t *d = (uint32_t *)dst, *s = (uint32_t *)src;
		size_t i, end = len / 4;

		for (i = 0; i < end; i++)
			d[i] = s[i];
	}
}

static bool mtl_copy(nsfb_t *nsfb, nsfb_bbox_t *srcbox, nsfb_bbox_t *dstbox)
{
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

	/* Find the superset of the two boxes. */
	nsfb_plot_add_rect(srcbox, dstbox, &allbox);

	/* Clear the cursor if its within the region to be altered. */
	if ((cursor != NULL) && (cursor->plotted == true) &&
		(nsfb_plot_bbox_intersect(&allbox, &cursor->loc))) {
		nsfb_cursor_clear(nsfb, cursor);
	}

	srcptr = (nsfb->ptr + (srcy * nsfb->linelen) +
			 ((srcx * nsfb->bpp) / 8));
	dstptr = (nsfb->ptr + (dsty * nsfb->linelen) +
			 ((dstx * nsfb->bpp) / 8));
	if (width == nsfb->width) {
		/*
		 * The box width is the same as the buffer.
		 * Just use memmove.
		 */
		pixelmove(dstptr, srcptr, (width * height * nsfb->bpp) / 8, nsfb->bpp);
	} else {
		if (srcy > dsty) {
			for (hloop = height; hloop > 0; hloop--) {
				pixelmove(dstptr, srcptr, (width * nsfb->bpp) / 8, nsfb->bpp);
				srcptr += nsfb->linelen;
				dstptr += nsfb->linelen;
			}
		} else {
			srcptr += height * nsfb->linelen;
			dstptr += height * nsfb->linelen;
			for (hloop = height; hloop > 0; hloop--) {
				srcptr -= nsfb->linelen;
				dstptr -= nsfb->linelen;
				pixelmove(dstptr, srcptr, (width * nsfb->bpp) / 8, nsfb->bpp);
			}
		}
	}

	if ((cursor != NULL) &&
		(cursor->plotted == false)) {
		nsfb_cursor_plot(nsfb, cursor);
	}

	return (true);
}

static int
mtl_set_geometry(nsfb_t *nsfb, int width, int height, enum nsfb_format_e format)
{
	UNUSED(width);
	UNUSED(height);
	UNUSED(format);

	/* We only support one geometry. */
	nsfb->width = MTL_RES_WIDTH;
	nsfb->height = MTL_RES_HEIGHT;
	nsfb->format = MTL_FORMAT;

	/* select soft plotters appropriate for format */
	select_plotters(nsfb);

	nsfb->plotter_fns->copy = mtl_copy;

	return (0);
}


static int mtl_claim(nsfb_t *nsfb, nsfb_bbox_t *box)
{
	struct nsfb_cursor_s *cursor = nsfb->cursor;

	if ((cursor != NULL) &&
		(cursor->plotted == true) &&
		(nsfb_plot_bbox_intersect(box, &cursor->loc))) {
			nsfb_cursor_clear(nsfb, cursor);
	}
	return 0;
}

static int mtl_cursor(nsfb_t *nsfb, struct nsfb_cursor_s *cursor)
{
	nsfb_bbox_t sclip;

	if ((cursor != NULL) && (cursor->plotted == true)) {
		sclip = nsfb->clip;
		nsfb_cursor_clear(nsfb, cursor);
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
