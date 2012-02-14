/*
 * draw.c
 *
 * accept dvi function calls and translate to X
 */

#include <X11/Xos.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

/* math.h on a Sequent doesn't define M_PI, apparently */
#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

#include "DviP.h"

#define DeviceToX(dw, n) ((int)((n) * (dw)->dvi.scale_factor + .5))
#define XPos(dw) (DeviceToX((dw), (dw)->dvi.state->x - \
                  (dw)->dvi.text_device_width) + (dw)->dvi.text_x_width)
#define YPos(dw) (DeviceToX((dw), (dw)->dvi.state->y))

static int FakeCharacter(DviWidget, char *, int);

/* font.c */
extern int MaxFontPosition(DviWidget);

void
HorizontalMove(DviWidget dw, int delta)
{
	dw->dvi.state->x += delta;
}

void
HorizontalGoto(DviWidget dw, int NewPosition)
{
	dw->dvi.state->x = NewPosition;
}

void
VerticalMove(DviWidget dw, int delta)
{
	dw->dvi.state->y += delta;
}

void
VerticalGoto(DviWidget dw, int NewPosition)
{
	dw->dvi.state->y = NewPosition;
}

void
AdjustCacheDeltas (DviWidget dw)
{
	int extra;
	int nadj;
	int i;

	nadj = 0;
	extra = DeviceToX(dw, dw->dvi.text_device_width)
		- dw->dvi.text_x_width;
	if (extra == 0)
		return;
	for (i = 0; i <= dw->dvi.cache.index; i++)
		if (dw->dvi.cache.adjustable[i])
			++nadj;
	dw->dvi.text_x_width += extra;
	if (nadj <= 1)
		return;
	for (i = 0; i <= dw->dvi.cache.index; i++)
		if (dw->dvi.cache.adjustable[i]) {
			int x;
			int *deltap;

			x = extra/nadj;
			deltap = &dw->dvi.cache.cache[i].delta;
#define MIN_DELTA 2
			if (*deltap > 0 && x + *deltap < MIN_DELTA) {
				x = MIN_DELTA - *deltap;
				if (x <= 0)
					*deltap = MIN_DELTA;
				else
					x = 0;
			}
			else
				*deltap += x;
			extra -= x;
			--nadj;
			dw->dvi.cache.adjustable[i] = 0;
		}
}

void
FlushCharCache (DviWidget dw)
{
	if (dw->dvi.cache.char_index != 0) {
		AdjustCacheDeltas (dw);
		XDrawText (XtDisplay (dw), XtWindow (dw), dw->dvi.normal_GC,
			   dw->dvi.cache.start_x, dw->dvi.cache.start_y,
			   dw->dvi.cache.cache, dw->dvi.cache.index + 1);
	}	
	dw->dvi.cache.index = 0;
	dw->dvi.cache.max = DVI_TEXT_CACHE_SIZE;
#if 0
	if (dw->dvi.noPolyText)
	    dw->dvi.cache.max = 1;
#endif
	dw->dvi.cache.char_index = 0;
	dw->dvi.cache.cache[0].nchars = 0;
	dw->dvi.cache.start_x = dw->dvi.cache.x	= XPos (dw);
	dw->dvi.cache.start_y = dw->dvi.cache.y = YPos (dw);
}

void
Newline (DviWidget dw)
{
	FlushCharCache (dw);
	dw->dvi.text_x_width = dw->dvi.text_device_width = 0;
	dw->dvi.word_flag = 0;
}

void
Word (DviWidget dw)
{
	dw->dvi.word_flag = 1;
}

#define charWidth(fi,c) (\
    (fi)->per_char ?\
	(fi)->per_char[(c) - (fi)->min_char_or_byte2].width\
    :\
	(fi)->max_bounds.width\
)
 

static
int charExists (XFontStruct *fi, int c)
{
	XCharStruct *p;

	/* `c' is always >= 0 */
	if (fi->per_char == NULL
	    || (unsigned int)c < fi->min_char_or_byte2
	    || (unsigned int)c > fi->max_char_or_byte2)
		return 0;
	p = fi->per_char + (c - fi->min_char_or_byte2);
	return (p->lbearing != 0 || p->rbearing != 0 || p->width != 0
		|| p->ascent != 0 || p->descent != 0 || p->attributes != 0);
}

/* `wid' is in device units */
static void
DoCharacter (DviWidget dw, int c, int wid)
{
	register XFontStruct	*font;
	register XTextItem	*text;
	int	x, y;
	
	x = XPos(dw);
	y = YPos(dw);

	/*
	 * quick and dirty extents calculation:
	 */
	if (!(y + 24 >= dw->dvi.extents.y1
	      && y - 24 <= dw->dvi.extents.y2
#if 0
	      && x + 24 >= dw->dvi.extents.x1
	      && x - 24 <= dw->dvi.extents.x2
#endif
	    ))
		return;
	
	if (y != dw->dvi.cache.y
	    || dw->dvi.cache.char_index >= DVI_CHAR_CACHE_SIZE) {
		FlushCharCache (dw);
		x = dw->dvi.cache.x;
		dw->dvi.cache.adjustable[dw->dvi.cache.index] = 0;
	}
	/*
	 * load a new font, if the current block is not empty,
	 * step to the next.
	 */
	if (dw->dvi.cache.font_size != dw->dvi.state->font_size ||
	    dw->dvi.cache.font_number != dw->dvi.state->font_number)
	{
		FlushCharCache (dw);
		x = dw->dvi.cache.x;
		dw->dvi.cache.font_size = dw->dvi.state->font_size;
		dw->dvi.cache.font_number = dw->dvi.state->font_number;
		dw->dvi.cache.font = QueryFont (dw,
						dw->dvi.cache.font_number,
						dw->dvi.cache.font_size);
		if (dw->dvi.cache.cache[dw->dvi.cache.index].nchars != 0) {
			++dw->dvi.cache.index;
			if (dw->dvi.cache.index >= dw->dvi.cache.max)
				FlushCharCache (dw);
			dw->dvi.cache.cache[dw->dvi.cache.index].nchars = 0;
			dw->dvi.cache.adjustable[dw->dvi.cache.index] = 0;
		}
	}
	if (x != dw->dvi.cache.x || dw->dvi.word_flag) {
		if (dw->dvi.cache.cache[dw->dvi.cache.index].nchars != 0) {
			++dw->dvi.cache.index;
			if (dw->dvi.cache.index >= dw->dvi.cache.max)
				FlushCharCache (dw);
			dw->dvi.cache.cache[dw->dvi.cache.index].nchars = 0;
			dw->dvi.cache.adjustable[dw->dvi.cache.index] = 0;
		}
		dw->dvi.cache.adjustable[dw->dvi.cache.index]
			= dw->dvi.word_flag;
		dw->dvi.word_flag = 0;
	}
	font = dw->dvi.cache.font;
	text = &dw->dvi.cache.cache[dw->dvi.cache.index];
	if (text->nchars == 0) {
		text->chars = &dw->dvi.cache.char_cache[dw->dvi.cache.char_index];
		text->delta = x - dw->dvi.cache.x;
		if (font != dw->dvi.font) {
			text->font = font->fid;
			dw->dvi.font = font;
		} else
			text->font = None;
		dw->dvi.cache.x += text->delta;
	}
	if (charExists(font, c)) {
		int w;
		dw->dvi.cache.char_cache[dw->dvi.cache.char_index++] = (char) c;
		++text->nchars;
		w = charWidth(font, c);
		dw->dvi.cache.x += w;
		if (wid != 0) {
			dw->dvi.text_x_width += w;
			dw->dvi.text_device_width += wid;
		}
	}
}

static
int FindCharWidth (DviWidget dw, char *buf, int *widp)
{
	int maxpos;
	int i;

	if (dw->dvi.device_font == 0
	    || dw->dvi.state->font_number != dw->dvi.device_font_number) {
		dw->dvi.device_font_number = dw->dvi.state->font_number;
		dw->dvi.device_font
			= QueryDeviceFont (dw, dw->dvi.device_font_number);
	}
	if (dw->dvi.device_font
	    && device_char_width (dw->dvi.device_font,
				  dw->dvi.state->font_size, buf, widp))
		return 1;

	maxpos = MaxFontPosition (dw);
	for (i = 1; i <= maxpos; i++) {
		DeviceFont *f = QueryDeviceFont (dw, i);
		if (f && device_font_special (f)
		    && device_char_width (f, dw->dvi.state->font_size,
					  buf, widp)) {
			dw->dvi.state->font_number = i;
			return 1;
		}
	}
	return 0;
}

/* Return the width of the character in device units. */

int PutCharacter (DviWidget dw, char *buf)
{
	int		prevFont;
	int		c = -1;
	int		wid = 0;
	DviCharNameMap	*map;

	if (!dw->dvi.display_enable)
		return 0;	/* The width doesn't matter in this case. */
	prevFont = dw->dvi.state->font_number;
	if (!FindCharWidth (dw, buf, &wid))
		return 0;
	map = QueryFontMap (dw, dw->dvi.state->font_number);
	if (map)
		c = DviCharIndex (map, buf);
	if (c >= 0)
		DoCharacter (dw, c, wid);
	else
		(void) FakeCharacter (dw, buf, wid);
	dw->dvi.state->font_number = prevFont;
	return wid;
}

/* Return 1 if we can fake it; 0 otherwise. */

static
int FakeCharacter (DviWidget dw, char *buf, int wid)
{
	int oldx, oldw;
	char ch[2];
	const char *chars = 0;

	if (buf[0] == '\0' || buf[1] == '\0' || buf[2] != '\0')
		return 0;
#define pack2(c1, c2) (((c1) << 8) | (c2))

	switch (pack2(buf[0], buf[1])) {
	case pack2('f', 'i'):
		chars = "fi";
		break;
	case pack2('f', 'l'):
		chars = "fl";
		break;
	case pack2('f', 'f'):
		chars = "ff";
		break;
	case pack2('F', 'i'):
		chars = "ffi";
		break;
	case pack2('F', 'l'):
		chars = "ffl";
		break;
	}
	if (!chars)
		return 0;
	oldx = dw->dvi.state->x;
	oldw = dw->dvi.text_device_width;
	ch[1] = '\0';
	for (; *chars; chars++) {
		ch[0] = *chars;
		dw->dvi.state->x += PutCharacter (dw, ch);
	}
	dw->dvi.state->x = oldx;
	dw->dvi.text_device_width = oldw + wid;
	return 1;
}

void
PutNumberedCharacter (DviWidget dw, int c)
{
	char *name;
	int wid;
	DviCharNameMap	*map;

	if (!dw->dvi.display_enable)
		return;

	if (dw->dvi.device_font == 0
	    || dw->dvi.state->font_number != dw->dvi.device_font_number) {
		dw->dvi.device_font_number = dw->dvi.state->font_number;
		dw->dvi.device_font
			= QueryDeviceFont (dw, dw->dvi.device_font_number);
	}
	
	if (dw->dvi.device_font == 0
	    || !device_code_width (dw->dvi.device_font,
				   dw->dvi.state->font_size, c, &wid))
		return;
	if (dw->dvi.native) {
		DoCharacter (dw, c, wid);
		return;
	}
	map = QueryFontMap (dw, dw->dvi.state->font_number);
	if (!map)
		return;
	for (name = device_name_for_code (dw->dvi.device_font, c);
	     name;
	     name = device_name_for_code ((DeviceFont *)0, c)) {
		int code = DviCharIndex (map, name);
		if (code >= 0) {
			DoCharacter (dw, code, wid);
			break;
		}
		if (FakeCharacter (dw, name, wid))
			break;
	}
}

void
ClearPage (DviWidget dw)
{
	XClearWindow (XtDisplay (dw), XtWindow (dw));
}

static void
setGC (DviWidget dw)
{
	int desired_line_width;
	
	if (dw->dvi.line_thickness < 0)
		desired_line_width = (int)(((double)dw->dvi.device_resolution
					    * dw->dvi.state->font_size)
					   / (10.0*72.0*dw->dvi.sizescale));
	else
		desired_line_width = dw->dvi.line_thickness;
	
	if (desired_line_width != dw->dvi.line_width) {
		XGCValues values;
		values.line_width = DeviceToX(dw, desired_line_width);
		if (values.line_width == 0)
			values.line_width = 1;
		XChangeGC(XtDisplay (dw), dw->dvi.normal_GC,
			  GCLineWidth, &values);
		dw->dvi.line_width = desired_line_width;
	}
}

static void
setFillGC (DviWidget dw)
{
	int fill_type;
	unsigned long mask = GCFillStyle | GCForeground;

	fill_type = (dw->dvi.fill * 10) / (DVI_FILL_MAX + 1);
	if (dw->dvi.fill_type != fill_type) {
		XGCValues values;
		if (fill_type <= 0) {
			values.foreground = dw->dvi.background;
			values.fill_style = FillSolid;
		} else if (fill_type >= 9) {
			values.foreground = dw->dvi.foreground;
			values.fill_style = FillSolid;
		} else {
			values.foreground = dw->dvi.foreground;
			values.fill_style = FillOpaqueStippled;
			values.stipple = dw->dvi.gray[fill_type - 1];
			mask |= GCStipple;
		}
		XChangeGC(XtDisplay (dw), dw->dvi.fill_GC, mask, &values);
		dw->dvi.fill_type = fill_type;
	}
}

void
DrawLine (DviWidget dw, int x, int y)
{
	int xp, yp;

	AdjustCacheDeltas (dw);
	setGC (dw);
	xp = XPos (dw);
	yp = YPos (dw);
	XDrawLine (XtDisplay (dw), XtWindow (dw), dw->dvi.normal_GC,
		   xp, yp,
		   xp + DeviceToX (dw, x), yp + DeviceToX (dw, y));
}

void
DrawCircle (DviWidget dw, int diam)
{
	int d;

	AdjustCacheDeltas (dw);
	setGC (dw);
	d = DeviceToX (dw, diam);
	XDrawArc (XtDisplay (dw), XtWindow (dw), dw->dvi.normal_GC,
		  XPos (dw), YPos (dw) - d/2,
		  d, d, 0, 64*360);
}

void
DrawFilledCircle (DviWidget dw, int diam)
{
	int d;

	AdjustCacheDeltas (dw);
	setFillGC (dw);
	d = DeviceToX (dw, diam);
	XFillArc (XtDisplay (dw), XtWindow (dw), dw->dvi.fill_GC,
		  XPos (dw), YPos (dw) - d/2,
		  d, d, 0, 64*360);
	XDrawArc (XtDisplay (dw), XtWindow (dw), dw->dvi.fill_GC,
		  XPos (dw), YPos (dw) - d/2,
		  d, d, 0, 64*360);
}

void
DrawEllipse (DviWidget dw, int a, int b)
{
	AdjustCacheDeltas (dw);
	setGC (dw);
	XDrawArc (XtDisplay (dw), XtWindow (dw), dw->dvi.normal_GC,
		  XPos (dw), YPos (dw) - DeviceToX (dw, b/2),
		  DeviceToX (dw, a), DeviceToX (dw, b), 0, 64*360);
}

void
DrawFilledEllipse (DviWidget dw, int a, int b)
{
	AdjustCacheDeltas (dw);
	setFillGC (dw);
	XFillArc (XtDisplay (dw), XtWindow (dw), dw->dvi.fill_GC,
		  XPos (dw), YPos (dw) - DeviceToX (dw, b/2),
		  DeviceToX (dw, a), DeviceToX (dw, b), 0, 64*360);
	XDrawArc (XtDisplay (dw), XtWindow (dw), dw->dvi.fill_GC,
		  XPos (dw), YPos (dw) - DeviceToX (dw, b/2),
		  DeviceToX (dw, a), DeviceToX (dw, b), 0, 64*360);
}

void
DrawArc (DviWidget dw, int x_0, int y_0, int x_1, int y_1)
{
	int angle1, angle2;
	int rad = (int)((sqrt ((double)x_0*x_0 + (double)y_0*y_0)
			 + sqrt ((double)x_1*x_1 + (double)y_1*y_1)
			 + 1.0)/2.0);
	if ((x_0 == 0 && y_0 == 0) || (x_1 == 0 && y_1 == 0))
		return;
	angle1 = (int)(atan2 ((double)y_0, (double)-x_0)*180.0*64.0/M_PI);
	angle2 = (int)(atan2 ((double)-y_1, (double)x_1)*180.0*64.0/M_PI);
	
	angle2 -= angle1;
	if (angle2 < 0)
		angle2 += 64*360;
	
	AdjustCacheDeltas (dw);
	setGC (dw);

	rad = DeviceToX (dw, rad);
	XDrawArc (XtDisplay (dw), XtWindow (dw), dw->dvi.normal_GC,
		  XPos (dw) + DeviceToX (dw, x_0) - rad,
		  YPos (dw) + DeviceToX (dw, y_0) - rad,
		  rad*2, rad*2, angle1, angle2);
}

void
DrawPolygon (DviWidget dw, int *v, int n)
{
	XPoint *p;
	int i;
	int dx, dy;
	
	n /= 2;
	
	AdjustCacheDeltas (dw);
	setGC (dw);
	p = (XPoint *)XtMalloc((n + 2)*sizeof(XPoint));
	p[0].x = XPos (dw);
	p[0].y = YPos (dw);
	dx = 0;
	dy = 0;
	for (i = 0; i < n; i++) {
		dx += v[2*i];
		p[i + 1].x = DeviceToX (dw, dx) + p[0].x;
		dy += v[2*i + 1];
		p[i + 1].y = DeviceToX (dw, dy) + p[0].y;
	}
	p[n+1].x = p[0].x;
	p[n+1].y = p[0].y;
	XDrawLines (XtDisplay (dw), XtWindow (dw), dw->dvi.normal_GC,
		   p, n + 2, CoordModeOrigin);
	XtFree((char *)p);
}

void
DrawFilledPolygon (DviWidget dw, int *v, int n)
{
	XPoint *p;
	int i;
	int dx, dy;
	
	n /= 2;
	if (n < 2)
		return;
	
	AdjustCacheDeltas (dw);
	setFillGC (dw);
	p = (XPoint *)XtMalloc((n + 2)*sizeof(XPoint));
	p[0].x = p[n+1].x = XPos (dw);
	p[0].y = p[n+1].y = YPos (dw);
	dx = 0;
	dy = 0;
	for (i = 0; i < n; i++) {
		dx += v[2*i];
		p[i + 1].x = DeviceToX (dw, dx) + p[0].x;
		dy += v[2*i + 1];
		p[i + 1].y = DeviceToX (dw, dy) + p[0].y;
	}
	XFillPolygon (XtDisplay (dw), XtWindow (dw), dw->dvi.fill_GC,
		      p, n + 1, Complex, CoordModeOrigin);
	XDrawLines (XtDisplay (dw), XtWindow (dw), dw->dvi.fill_GC,
		      p, n + 2, CoordModeOrigin);
	XtFree((char *)p);
}

#define POINTS_MAX 10000

static void
appendPoint(XPoint *points, int *pointi, int x, int y)
{
	if (*pointi < POINTS_MAX) {
		points[*pointi].x = x;
		points[*pointi].y = y;
		*pointi += 1;
	}
}

#define FLATNESS 1

static void
flattenCurve(XPoint *points, int *pointi,
	     int x_2, int y_2, int x_3, int y_3, int x_4, int y_4)
{
	int x_1, y_1, dx, dy, n1, n2, n;

	x_1 = points[*pointi - 1].x;
	y_1 = points[*pointi - 1].y;
	
	dx = x_4 - x_1;
	dy = y_4 - y_1;
	
	n1 = dy*(x_2 - x_1) - dx*(y_2 - y_1);
	n2 = dy*(x_3 - x_1) - dx*(y_3 - y_1);
	if (n1 < 0)
		n1 = -n1;
	if (n2 < 0)
		n2 = -n2;
	n = n1 > n2 ? n1 : n2;

	if (n*n / (dy*dy + dx*dx) <= FLATNESS*FLATNESS)
		appendPoint (points, pointi, x_4, y_4);
	else {
		flattenCurve (points, pointi,
			      (x_1 + x_2)/2,
			      (y_1 + y_2)/2,
			      (x_1 + x_2*2 + x_3)/4,
			      (y_1 + y_2*2 + y_3)/4,
			      (x_1 + 3*x_2 + 3*x_3 + x_4)/8,
			      (y_1 + 3*y_2 + 3*y_3 + y_4)/8);
		flattenCurve (points, pointi,
			      (x_2 + x_3*2 + x_4)/4,
			      (y_2 + y_3*2 + y_4)/4,
			      (x_3 + x_4)/2,
			      (y_3 + y_4)/2,
			      x_4,
			      y_4);
	}
}

void
DrawSpline (DviWidget dw, int *v, int n)
{
	int sx, sy, tx, ty;
	int ox, oy, dx, dy;
	int i;
	int pointi;
	XPoint points[POINTS_MAX];
	
	if (n == 0 || (n & 1) != 0)
		return;
	AdjustCacheDeltas (dw);
	setGC (dw);
	ox = XPos (dw);
	oy = YPos (dw);
	dx = v[0];
	dy = v[1];
	sx = ox;
	sy = oy;
	tx = sx + DeviceToX (dw, dx);
	ty = sy + DeviceToX (dw, dy);
	
	pointi = 0;
	
	appendPoint (points, &pointi, sx, sy);
	appendPoint (points, &pointi, (sx + tx)/2, (sy + ty)/2);
	
	for (i = 2; i < n; i += 2) {
		int ux = ox + DeviceToX (dw, dx += v[i]);
		int uy = oy + DeviceToX (dw, dy += v[i+1]);
		flattenCurve (points, &pointi,
			       (sx + tx*5)/6, (sy + ty*5)/6,
			       (tx*5 + ux)/6, (ty*5 + uy)/6,
			       (tx + ux)/2, (ty + uy)/2);
		sx = tx;
		sy = ty;
		tx = ux;
		ty = uy;
	}
	
	appendPoint (points, &pointi, tx, ty);
	
	XDrawLines (XtDisplay (dw), XtWindow (dw), dw->dvi.normal_GC,
		   points, pointi, CoordModeOrigin);
}


/*
Local Variables:
c-indent-level: 8
c-continued-statement-offset: 8
c-brace-offset: -8
c-argdecl-indent: 8
c-label-offset: -8
c-tab-always-indent: nil
End:
*/
