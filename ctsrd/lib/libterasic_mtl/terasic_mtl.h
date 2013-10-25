/*-
 * Copyright (c) 2012 Simon W. Moore
 * Copyright (c) 2012 SRI International
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

#ifndef _DE4TC_H_
#define _DE4TC_H_

#define	TSG_NONE	0x00
#define	TSG_NORTH	0x10
#define	TSG_NORTHEAST	0x12
#define	TSG_EAST	0x14
#define	TSG_SOUTHEAST	0x16
#define	TSG_SOUTH	0x18
#define	TSG_SOUTHWEST	0x1A
#define	TSG_WEST	0x1C
#define	TSG_NORTHWEST	0x1E
#define	TSG_ROTATE_CW	0x28	/* Clockwise */
#define	TSG_ROTATE_CCW	0x29	/* Counter Clockwise */
#define	TSG_CLICK	0x20
#define	TSG_DCLICK	0x22	/* Double Click */
#define	TSG2_NORTH	0x30
#define	TSG2_NORTHEAST	0x32
#define	TSG2_EAST	0x34
#define	TSG2_SOUTHEAST	0x36
#define	TSG2_SOUTH	0x38
#define	TSG2_SOUTHWEST	0x3A
#define	TSG2_WEST	0x3C
#define	TSG2_NORTHWEST	0x3E
#define	TSG2_CLICK	0x40
#define	TSG2_ZOOM_IN	0x48
#define	TSG2_ZOOM_OUT	0x49

#define	TSGF_NONE	0x00000001
#define	TSGF_NORTH	0x00000002
#define	TSGF_NORTHEAST	0x00000004
#define	TSGF_EAST	0x00000008
#define	TSGF_SOUTHEAST	0x00000010
#define	TSGF_SOUTH	0x00000020
#define	TSGF_SOUTHWEST	0x00000040
#define	TSGF_WEST	0x00000080
#define	TSGF_NORTHWEST	0x00000100
#define	TSGF_ROTATE_CW	0x00000200
#define	TSGF_ROTATE_CCW	0x00000400
#define	TSGF_CLICK	0x00000800
#define	TSGF_DCLICK	0x00001000
#define	TSGF_2NORTH	0x00002000
#define	TSGF_2NORTHEAST	0x00004000
#define	TSGF_2EAST	0x00008000
#define	TSGF_2SOUTHEAST	0x00010000
#define	TSGF_2SOUTH	0x00020000
#define	TSGF_2SOUTHWEST	0x00040000
#define	TSGF_2WEST	0x00080000
#define	TSGF_2NORTHWEST	0x00100000
#define	TSGF_2CLICK	0x00200000
#define	TSGF_ZOOM_IN	0x00400000
#define	TSGF_ZOOM_OUT	0x00800000

typedef enum {
	FBDA_CANCEL,
	FBDA_OK,
	FBDA_YES,
	FBDA_NO,
	FBDA_DOWN,
	FBDA_UP
} fb_dialog_action;
        
typedef enum {
	FBDT_EAST2CLOSE,
	FBDT_PINCH2CLOSE,
	FBDT_PINCH_OR_VSCROLL,
#ifdef NOTYET
	FBDT_OK,
	FBDT_OKCANCEL,
	FBDT_YESNO
#endif
} fb_dialog_type;

struct tsstate {
	int ts_x1;
	int ts_y1;
	int ts_x2;
	int ts_y2;
	int ts_count;
	int ts_gesture;
};

extern int touch_x0;
extern int touch_y0;
extern int touch_x1;
extern int touch_y1;
extern int touch_gesture;
extern int touch_count;

extern const int fb_height;
extern const int fb_width;

extern volatile u_int32_t *pfbp;
extern volatile u_int32_t *mtlctrl;

void multitouch_pole(void);
void multitouch_filter(void);
void multitouch_release_event(void);
struct tsstate* ts_poll(void);
void ts_drain(void);
int tsg2tsgf(int g);
int tsgf2tsg(int f);
void fb_init(void);
void fb_fini(void);
u_int32_t fb_colour(int r, int g, int b);
void fb_putpixel(int px, int py, int colour);
int fb_composite(u_int32_t *dbuf, int dwidth, int dheight, int x, int y,
    const u_int32_t *sbuf, int swidth, int sheight);
void fb_rectangle(u_int32_t color, int thickness, int x, int y, int
    width, int height);
void fb_fill(int col);
void fb_fill_region(u_int32_t colour, int x, int y, int w, int h);
void fb_fill_buf(u_int32_t *buf, u_int32_t color, int width, int height);
void fb_post(u_int32_t *buf);
void fb_post_region(u_int32_t *buf, int x, int y, int w, int h);
void fb_save(u_int32_t *buf);
void fb_blend(int blend_text_bg, int blend_text_fg, int blend_pixel, int wash);
void fb_text_cursor(int x, int y);
void fb_fade2off(void);
void fb_fade2on(void);
void fb_fade2text(int textbg_alpha);

void plot_line(int x1, int y1, int x2, int y2, unsigned int colour);
int read_png_file(const char* file_name, u_int32_t* imgbuf, int maxwidth, int maxheight);
int read_png_fd(int fd, u_int32_t* imgbuf, int maxwidth, int maxheight);

void busy_indicator(void);

void fb_load_syscons_font(const char *type, const char *filename);
int fb_get_font_height(void);
int fb_get_font_width(void);
void fb_render_text(const char *string, int expand, u_int32_t con, u_int32_t coff,
    u_int32_t *buffer, int w, int h);
fb_dialog_action fb_dialog(fb_dialog_type type, u_int32_t bcolor,
    u_int32_t bgcolor, u_int32_t tcolor, const char *title, const char *text);
int fb_dialog_gestures(int gestures, u_int32_t bcolor, u_int32_t bgcolor,
    u_int32_t tcolor, const char *title, const char *text);

#endif /* !_DE4TC_H_ */
