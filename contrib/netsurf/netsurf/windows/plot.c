/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "utils/config.h"

#include <windows.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "desktop/gui.h"
#include "desktop/plotters.h"

#include "windows/bitmap.h"
#include "windows/font.h"
#include "windows/gui.h"
#include "windows/plot.h"


/* set NSWS_PLOT_DEBUG to 0 for no debugging, 1 for debugging */
/* #define NSWS_PLOT_DEBUG  */

#ifdef NSWS_PLOT_DEBUG
#define PLOT_LOG(x) LOG(x)
#else
#define PLOT_LOG(x)
#endif

HDC plot_hdc;

static RECT plot_clip; /* currently set clipping rectangle */

static bool clip(const struct rect *clip)
{
	PLOT_LOG(("clip %d,%d to %d,%d", clip->x0, clip->y0, clip->x1, clip->y1));

	plot_clip.left = clip->x0;
	plot_clip.top = clip->y0;
	plot_clip.right = clip->x1 + 1; /* co-ordinates are exclusive */
	plot_clip.bottom = clip->y1 + 1; /* co-ordinates are exclusive */

	return true;
}

static bool line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	PLOT_LOG(("from %d,%d to %d,%d", x0, y0, x1, y1));

	/* ensure the plot HDC is set */
	if (plot_hdc == NULL) {
		LOG(("HDC not set on call to plotters"));
		return false;
	}

	HRGN clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	COLORREF col = (DWORD)(style->stroke_colour & 0x00FFFFFF);
	/* windows 0x00bbggrr */
	DWORD penstyle = PS_GEOMETRIC | ((style->stroke_type ==
					  PLOT_OP_TYPE_DOT) ? PS_DOT :
					 (style->stroke_type == PLOT_OP_TYPE_DASH) ? PS_DASH:
					 0);
	LOGBRUSH lb = {BS_SOLID, col, 0};
	HPEN pen = ExtCreatePen(penstyle, style->stroke_width, &lb, 0, NULL);
	if (pen == NULL) {
		DeleteObject(clipregion);
		return false;
	}
	HGDIOBJ bak = SelectObject(plot_hdc, (HGDIOBJ) pen);
	if (bak == NULL) {
		DeleteObject(pen);
		DeleteObject(clipregion);
		return false;
	}
/*
	RECT r;
	r.left = x0;
	r.top = y0;
	r.right = x1;
	r.bottom = y1;
*/
	SelectClipRgn(plot_hdc, clipregion);

	MoveToEx(plot_hdc, x0, y0, (LPPOINT) NULL);

	LineTo(plot_hdc, x1, y1);

	SelectClipRgn(plot_hdc, NULL);
	pen = SelectObject(plot_hdc, bak);

	DeleteObject(pen);
	DeleteObject(clipregion);

	return true;
}

static bool rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	PLOT_LOG(("rectangle from %d,%d to %d,%d", x0, y0, x1, y1));

	/* ensure the plot HDC is set */
	if (plot_hdc == NULL) {
		LOG(("HDC not set on call to plotters"));
		return false;
	}

	HRGN clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	x1++;
	y1++;

	COLORREF pencol = (DWORD)(style->stroke_colour & 0x00FFFFFF);
	DWORD penstyle = PS_GEOMETRIC |
		(style->stroke_type == PLOT_OP_TYPE_DOT ? PS_DOT :
		 (style->stroke_type == PLOT_OP_TYPE_DASH ? PS_DASH :
		  (style->stroke_type == PLOT_OP_TYPE_NONE ? PS_NULL :
		   0)));
	LOGBRUSH lb = {BS_SOLID, pencol, 0};
	LOGBRUSH lb1 = {BS_SOLID, style->fill_colour, 0};
	if (style->fill_type == PLOT_OP_TYPE_NONE)
		lb1.lbStyle = BS_HOLLOW;

	HPEN pen = ExtCreatePen(penstyle, style->stroke_width, &lb, 0, NULL);
	if (pen == NULL) {
		return false;
	}
	HGDIOBJ penbak = SelectObject(plot_hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(pen);
		return false;
	}
	HBRUSH brush = CreateBrushIndirect(&lb1);
	if (brush  == NULL) {
		SelectObject(plot_hdc, penbak);
		DeleteObject(pen);
		return false;
	}
	HGDIOBJ brushbak = SelectObject(plot_hdc, (HGDIOBJ) brush);
	if (brushbak == NULL) {
		SelectObject(plot_hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);
		return false;
	}

	SelectClipRgn(plot_hdc, clipregion);

	Rectangle(plot_hdc, x0, y0, x1, y1);

	pen = SelectObject(plot_hdc, penbak);
	brush = SelectObject(plot_hdc, brushbak);
	SelectClipRgn(plot_hdc, NULL);
	DeleteObject(pen);
	DeleteObject(brush);
	DeleteObject(clipregion);

	return true;
}


static bool polygon(const int *p, unsigned int n, const plot_style_t *style)
{
	PLOT_LOG(("polygon %d points", n));

	/* ensure the plot HDC is set */
	if (plot_hdc == NULL) {
		LOG(("HDC not set on call to plotters"));
		return false;
	}

	POINT points[n];
	unsigned int i;
	HRGN clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	COLORREF pencol = (DWORD)(style->fill_colour & 0x00FFFFFF);
	COLORREF brushcol = (DWORD)(style->fill_colour & 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_NULL, 1, pencol);
	if (pen == NULL) {
		DeleteObject(clipregion);
		return false;
	}
	HPEN penbak = SelectObject(plot_hdc, pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);
		return false;
	}
	HBRUSH brush = CreateSolidBrush(brushcol);
	if (brush == NULL) {
		DeleteObject(clipregion);
		SelectObject(plot_hdc, penbak);
		DeleteObject(pen);
		return false;
	}
	HBRUSH brushbak = SelectObject(plot_hdc, brush);
	if (brushbak == NULL) {
		DeleteObject(clipregion);
		SelectObject(plot_hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);
		return false;
	}
	SetPolyFillMode(plot_hdc, WINDING);
	for (i = 0; i < n; i++) {
		points[i].x = (long) p[2 * i];
		points[i].y = (long) p[2 * i + 1];

		PLOT_LOG(("%ld,%ld ", points[i].x, points[i].y));
	}

	SelectClipRgn(plot_hdc, clipregion);

	if (n >= 2)
		Polygon(plot_hdc, points, n);

	SelectClipRgn(plot_hdc, NULL);

	pen = SelectObject(plot_hdc, penbak);
	brush = SelectObject(plot_hdc, brushbak);
	DeleteObject(clipregion);
	DeleteObject(pen);
	DeleteObject(brush);

	return true;
}


static bool text(int x, int y, const char *text, size_t length,
		 const plot_font_style_t *style)
{
	PLOT_LOG(("words %s at %d,%d", text, x, y));

	/* ensure the plot HDC is set */
	if (plot_hdc == NULL) {
		LOG(("HDC not set on call to plotters"));
		return false;
	}

	HRGN clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	HFONT fontbak, font = get_font(style);
	if (font == NULL) {
		DeleteObject(clipregion);
		return false;
	}
	int wlen;
	SIZE s;
	LPWSTR wstring;
	fontbak = (HFONT) SelectObject(plot_hdc, font);
	GetTextExtentPoint(plot_hdc, text, length, &s);

/*
	RECT r;
	r.left = x;
	r.top = y  - (3 * s.cy) / 4;
	r.right = x + s.cx;
	r.bottom = y + s.cy / 4;
*/
	SelectClipRgn(plot_hdc, clipregion);

	SetTextAlign(plot_hdc, TA_BASELINE | TA_LEFT);
	if ((style->background & 0xFF000000) != 0x01000000)
		/* 100% alpha */
		SetBkColor(plot_hdc, (DWORD) (style->background & 0x00FFFFFF));
	SetBkMode(plot_hdc, TRANSPARENT);
	SetTextColor(plot_hdc, (DWORD) (style->foreground & 0x00FFFFFF));

	wlen = MultiByteToWideChar(CP_UTF8, 0, text, length, NULL, 0);
	wstring = malloc(2 * (wlen + 1));
	if (wstring == NULL) {
		return false;
	}
	MultiByteToWideChar(CP_UTF8, 0, text, length, wstring, wlen);
	TextOutW(plot_hdc, x, y, wstring, wlen);

	SelectClipRgn(plot_hdc, NULL);
	free(wstring);
	font = SelectObject(plot_hdc, fontbak);
	DeleteObject(clipregion);
	DeleteObject(font);

	return true;
}

static bool disc(int x, int y, int radius, const plot_style_t *style)
{
	PLOT_LOG(("disc at %d,%d radius %d", x, y, radius));

	/* ensure the plot HDC is set */
	if (plot_hdc == NULL) {
		LOG(("HDC not set on call to plotters"));
		return false;
	}

	HRGN clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	COLORREF col = (DWORD)((style->fill_colour | style->stroke_colour)
			       & 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_SOLID, 1, col);
	if (pen == NULL) {
		DeleteObject(clipregion);
		return false;
	}
	HGDIOBJ penbak = SelectObject(plot_hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);
		return false;
	}
	HBRUSH brush = CreateSolidBrush(col);
	if (brush == NULL) {
		DeleteObject(clipregion);
		SelectObject(plot_hdc, penbak);
		DeleteObject(pen);
		return false;
	}
	HGDIOBJ brushbak = SelectObject(plot_hdc, (HGDIOBJ) brush);
	if (brushbak == NULL) {
		DeleteObject(clipregion);
		SelectObject(plot_hdc, penbak);
		DeleteObject(pen);
		DeleteObject(brush);
		return false;
	}
/*
	RECT r;
	r.left = x - radius;
	r.top = y - radius;
	r.right = x + radius;
	r.bottom = y + radius;
*/
	SelectClipRgn(plot_hdc, clipregion);

	if (style->fill_type == PLOT_OP_TYPE_NONE)
		Arc(plot_hdc, x - radius, y - radius, x + radius, y + radius,
		    x - radius, y - radius,
		    x - radius, y - radius);
	else
		Ellipse(plot_hdc, x - radius, y - radius, x + radius, y + radius);

	SelectClipRgn(plot_hdc, NULL);
	pen = SelectObject(plot_hdc, penbak);
	brush = SelectObject(plot_hdc, brushbak);
	DeleteObject(clipregion);
	DeleteObject(pen);
	DeleteObject(brush);

	return true;
}

static bool arc(int x, int y, int radius, int angle1, int angle2,
		const plot_style_t *style)
{
	PLOT_LOG(("arc centre %d,%d radius %d from %d to %d", x, y, radius,
	     angle1, angle2));

	/* ensure the plot HDC is set */
	if (plot_hdc == NULL) {
		LOG(("HDC not set on call to plotters"));
		return false;
	}

	HRGN clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	COLORREF col = (DWORD)(style->stroke_colour & 0x00FFFFFF);
	HPEN pen = CreatePen(PS_GEOMETRIC | PS_SOLID, 1, col);
	if (pen == NULL) {
		DeleteObject(clipregion);
		return false;
	}
	HGDIOBJ penbak = SelectObject(plot_hdc, (HGDIOBJ) pen);
	if (penbak == NULL) {
		DeleteObject(clipregion);
		DeleteObject(pen);
		return false;
	}

	int q1, q2;
	double a1=1.0, a2=1.0, b1=1.0, b2=1.0;
	q1 = (int) ((angle1 + 45) / 90) - 45;
	q2 = (int) ((angle2 + 45) / 90) - 45;
	while (q1 > 4)
		q1 -= 4;
	while (q2 > 4)
		q2 -= 4;
	while (q1 <= 0)
		q1 += 4;
	while (q2 <= 0)
		q2 += 4;
	angle1 = ((angle1 + 45) % 90) - 45;
	angle2 = ((angle2 + 45) % 90) - 45;

	switch(q1) {
	case 1:
		a1 = 1.0;
		b1 = -tan((M_PI / 180) * angle1);
		break;
	case 2:
		b1 = -1.0;
		a1 = -tan((M_PI / 180) * angle1);
		break;
	case 3:
		a1 = -1.0;
		b1 = tan((M_PI / 180) * angle1);
		break;
	case 4:
		b1 = 1.0;
		a1 = tan((M_PI / 180) * angle1);
		break;
	}

	switch(q2) {
	case 1:
		a2 = 1.0;
		b2 = -tan((M_PI / 180) * angle2);
		break;
	case 2:
		b2 = -1.0;
		a2 = -tan((M_PI / 180) * angle2);
		break;
	case 3:
		a2 = -1.0;
		b2 = tan((M_PI / 180) * angle2);
		break;
	case 4:
		b2 = 1.0;
		a2 = tan((M_PI / 180) * angle2);
		break;
	}

/*
	RECT r;
	r.left = x - radius;
	r.top = y - radius;
	r.right = x + radius;
	r.bottom = y + radius;
*/
	SelectClipRgn(plot_hdc, clipregion);

	Arc(plot_hdc, x - radius, y - radius, x + radius, y + radius,
	    x + (int)(a1 * radius), y + (int)(b1 * radius),
	    x + (int)(a2 * radius), y + (int)(b2 * radius));

	SelectClipRgn(plot_hdc, NULL);
	pen = SelectObject(plot_hdc, penbak);
	DeleteObject(clipregion);
	DeleteObject(pen);

	return true;
}

static bool
plot_block(COLORREF col, int x, int y, int width, int height)
{
	HRGN clipregion;
	HGDIOBJ original = NULL;

	/* Bail early if we can */
	if ((x >= plot_clip.right) ||
	    ((x + width) < plot_clip.left) ||
	    (y >= plot_clip.bottom) ||
	    ((y + height) < plot_clip.top)) {
		/* Image completely outside clip region */
		return true;	
	}	

	/* ensure the plot HDC is set */
	if (plot_hdc == NULL) {
		LOG(("HDC not set on call to plotters"));
		return false;
	}

	clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	SelectClipRgn(plot_hdc, clipregion);

	/* Saving the original pen object */
	original = SelectObject(plot_hdc,GetStockObject(DC_PEN)); 

	SelectObject(plot_hdc, GetStockObject(DC_PEN));
	SelectObject(plot_hdc, GetStockObject(DC_BRUSH));
	SetDCPenColor(plot_hdc, col);
	SetDCBrushColor(plot_hdc, col);
	Rectangle(plot_hdc, x, y, width, height);

	SelectObject(plot_hdc,original); /* Restoring the original pen object */

	DeleteObject(clipregion);

	return true;

}

/* blunt force truma way of achiving alpha blended plotting */
static bool 
plot_alpha_bitmap(HDC hdc, 
		  struct bitmap *bitmap, 
		  int x, int y, 
		  int width, int height)
{
#ifdef WINDOWS_GDI_ALPHA_WORKED
	BLENDFUNCTION blnd = {  AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA };
	HDC bmihdc;
	bool bltres;
	bmihdc = CreateCompatibleDC(hdc);
	SelectObject(bmihdc, bitmap->windib);
	bltres = AlphaBlend(hdc, 
			    x, y, 
			    width, height,
			    bmihdc,
			    0, 0, 
			    bitmap->width, bitmap->height,
			    blnd);
	DeleteDC(bmihdc);
	return bltres;
#else
	HDC Memhdc;
	BITMAPINFOHEADER bmih;
	int v, vv, vi, h, hh, width4, transparency;
	unsigned char alpha;
	bool isscaled = false; /* set if the scaled bitmap requires freeing */
	BITMAP MemBM;
	BITMAPINFO *bmi;
	HBITMAP MemBMh;

	PLOT_LOG(("%p bitmap %d,%d width %d height %d", bitmap, x, y, width, height));
	PLOT_LOG(("clipped %ld,%ld to %ld,%ld",plot_clip.left, plot_clip.top, plot_clip.right, plot_clip.bottom));

	Memhdc = CreateCompatibleDC(hdc);
	if (Memhdc == NULL) {
		return false;
	}

	if ((bitmap->width != width) || 
	    (bitmap->height != height)) {
		PLOT_LOG(("scaling from %d,%d to %d,%d", 
		     bitmap->width, bitmap->height, width, height));
		bitmap = bitmap_scale(bitmap, width, height);
		if (bitmap == NULL)
			return false;
		isscaled = true;
	}

	bmi = (BITMAPINFO *) malloc(sizeof(BITMAPINFOHEADER) +
				    (bitmap->width * bitmap->height * 4));
	if (bmi == NULL) {
		DeleteDC(Memhdc);
		return false;
	}

	MemBMh = CreateCompatibleBitmap(hdc, bitmap->width, bitmap->height);
	if (MemBMh == NULL){
		free(bmi);
		DeleteDC(Memhdc);
		return false;
	}

	/* save 'background' data for alpha channel work */
	SelectObject(Memhdc, MemBMh);
	BitBlt(Memhdc, 0, 0, bitmap->width, bitmap->height, hdc, x, y, SRCCOPY);
	GetObject(MemBMh, sizeof(BITMAP), &MemBM);

	bmih.biSize = sizeof(bmih);
	bmih.biWidth = bitmap->width;
	bmih.biHeight = bitmap->height;
	bmih.biPlanes = 1;
	bmih.biBitCount = 32;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 4 * bitmap->height * bitmap->width;
	bmih.biXPelsPerMeter = 3600; /* 100 dpi */
	bmih.biYPelsPerMeter = 3600;
	bmih.biClrUsed = 0;
	bmih.biClrImportant = 0;
	bmi->bmiHeader = bmih;

	GetDIBits(hdc, MemBMh, 0, bitmap->height, bmi->bmiColors, bmi,
		  DIB_RGB_COLORS);

	/* then load 'foreground' bits from bitmap->pixdata */

	width4 = bitmap->width * 4;
	for (v = 0, vv = 0, vi = (bitmap->height - 1) * width4;
	     v < bitmap->height;
	     v++, vv += bitmap->width, vi -= width4) {
		for (h = 0, hh = 0; h < bitmap->width; h++, hh += 4) {
			alpha = bitmap->pixdata[vi + hh + 3];
/* multiplication of alpha value; subject to profiling could be optional */
			if (alpha == 0xFF) {
				bmi->bmiColors[vv + h].rgbBlue =
					bitmap->pixdata[vi + hh + 2];
				bmi->bmiColors[vv + h].rgbGreen =
					bitmap->pixdata[vi + hh + 1];
				bmi->bmiColors[vv + h].rgbRed =
					bitmap->pixdata[vi + hh];
			} else if (alpha > 0) {
				transparency = 0x100 - alpha;
				bmi->bmiColors[vv + h].rgbBlue =
					(bmi->bmiColors[vv + h].rgbBlue
					 * transparency +
					 (bitmap->pixdata[vi + hh + 2]) *
					 alpha) >> 8;
				bmi->bmiColors[vv + h].rgbGreen =
					(bmi->bmiColors[vv + h].
					 rgbGreen
					 * transparency +
					 (bitmap->pixdata[vi + hh + 1]) *
					 alpha) >> 8;
				bmi->bmiColors[vv + h].rgbRed =
					(bmi->bmiColors[vv + h].rgbRed
					 * transparency +
					 bitmap->pixdata[vi + hh]
					 * alpha) >> 8;
			}
		}
	}
	SetDIBitsToDevice(hdc, x, y, bitmap->width, bitmap->height,
			  0, 0, 0, bitmap->height, 
			  (const void *) bmi->bmiColors,
			  bmi, DIB_RGB_COLORS);

	if (isscaled && bitmap && bitmap->pixdata) {
		free(bitmap->pixdata);
		free(bitmap);
	}

	free(bmi);
	DeleteObject(MemBMh);
	DeleteDC(Memhdc);
	return true;
#endif
}


static bool 
plot_bitmap(struct bitmap *bitmap, int x, int y, int width, int height)
{
	int bltres;
	HRGN clipregion;

	/* Bail early if we can */
	if ((x >= plot_clip.right) ||
	    ((x + width) < plot_clip.left) ||
	    (y >= plot_clip.bottom) ||
	    ((y + height) < plot_clip.top)) {
		/* Image completely outside clip region */
		return true;	
	}	

	/* ensure the plot HDC is set */
	if (plot_hdc == NULL) {
		LOG(("HDC not set on call to plotters"));
		return false;
	}

	clipregion = CreateRectRgnIndirect(&plot_clip);
	if (clipregion == NULL) {
		return false;
	}

	SelectClipRgn(plot_hdc, clipregion);

	if (bitmap->opaque) {
		/* opaque bitmap */
		if ((bitmap->width == width) && 
		    (bitmap->height == height)) {
			/* unscaled */
			bltres = SetDIBitsToDevice(plot_hdc,
						   x, y,
						   width, height,
						   0, 0,
						   0,
						   height,
						   bitmap->pixdata,
						   (BITMAPINFO *)bitmap->pbmi,
						   DIB_RGB_COLORS);
		} else {
			/* scaled */
			SetStretchBltMode(plot_hdc, COLORONCOLOR);
			bltres = StretchDIBits(plot_hdc, 
					       x, y, 
					       width, height,
					       0, 0, 
					       bitmap->width, bitmap->height,
					       bitmap->pixdata, 
					       (BITMAPINFO *)bitmap->pbmi, 
					       DIB_RGB_COLORS, 
					       SRCCOPY);


		}
	} else {
		/* Bitmap with alpha.*/
		bltres = plot_alpha_bitmap(plot_hdc, bitmap, x, y, width, height);
	}

	PLOT_LOG(("bltres = %d", bltres)); 

	DeleteObject(clipregion);

	return true;

}

static bool
windows_plot_bitmap(int x, int y,
		    int width, int height,
		    struct bitmap *bitmap, colour bg,
		    bitmap_flags_t flags)
{
	int xf,yf;
	bool repeat_x = (flags & BITMAPF_REPEAT_X);
	bool repeat_y = (flags & BITMAPF_REPEAT_Y);

	/* Bail early if we can */

	PLOT_LOG(("Plotting %p at %d,%d by %d,%d",bitmap, x,y,width,height));

	if (bitmap == NULL) {
		LOG(("Passed null bitmap!"));
		return true;
	}

	/* check if nothing to plot */
	if (width == 0 || height == 0)
		return true;

	/* x and y define coordinate of top left of of the initial explicitly
	 * placed tile. The width and height are the image scaling and the
	 * bounding box defines the extent of the repeat (which may go in all
	 * four directions from the initial tile).
	 */

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just plot it */
		if ((bitmap->width == 1) && (bitmap->height == 1)) {
			if ((*(bitmap->pixdata + 3) & 0xff) == 0) {
				return true;
			}
			return plot_block((*(COLORREF *)bitmap->pixdata) & 0xffffff, x, y, x + width, y + height);

		} else {
			return plot_bitmap(bitmap, x, y, width, height);
		}
	}

	/* Optimise tiled plots of 1x1 bitmaps by replacing with a flat fill
	 * of the area.  Can only be done when image is fully opaque. */
	if ((bitmap->width == 1) && (bitmap->height == 1)) {
		if ((*(COLORREF *)bitmap->pixdata & 0xff000000) != 0) {
			return plot_block((*(COLORREF *)bitmap->pixdata) & 0xffffff, 
					  plot_clip.left, 
					  plot_clip.top, 
					  plot_clip.right, 
					  plot_clip.bottom);
		}
	}

	/* Optimise tiled plots of bitmaps scaled to 1x1 by replacing with
	 * a flat fill of the area.  Can only be done when image is fully
	 * opaque. */
	if ((width == 1) && (height == 1)) {
		if (bitmap->opaque) {
			/** TODO: Currently using top left pixel. Maybe centre
			 *        pixel or average value would be better. */
			return plot_block((*(COLORREF *)bitmap->pixdata) & 0xffffff, 
					  plot_clip.left, 
					  plot_clip.top, 
					  plot_clip.right, 
					  plot_clip.bottom);
		}
	}

	PLOT_LOG(("Tiled plotting %d,%d by %d,%d",x,y,width,height));
	PLOT_LOG(("clipped %ld,%ld to %ld,%ld",plot_clip.left, plot_clip.top, plot_clip.right, plot_clip.bottom));

	/* get left most tile position */
	if (repeat_x)
		for (; x > plot_clip.left; x -= width);

	/* get top most tile position */
	if (repeat_y)
		for (; y > plot_clip.top; y -= height);

	PLOT_LOG(("repeat from %d,%d to %ld,%ld", x, y, plot_clip.right, plot_clip.bottom));

	/* tile down and across to extents */
	for (xf = x; xf < plot_clip.right; xf += width) {
		for (yf = y; yf < plot_clip.bottom; yf += height) {

			plot_bitmap(bitmap, xf, yf, width, height);
			if (!repeat_y)
				break;
		}
		if (!repeat_x)
	   		break;
	}
	return true;
}


static bool flush(void)
{
	PLOT_LOG(("flush unimplemented"));
	return true;
}

static bool path(const float *p, unsigned int n, colour fill, float width,
		 colour c, const float transform[6])
{
	PLOT_LOG(("path unimplemented"));
	return true;
}

const struct plotter_table win_plotters = {
	.rectangle = rectangle,
	.line = line,
	.polygon = polygon,
	.clip = clip,
	.text = text,
	.disc = disc,
	.arc = arc,
	.bitmap = windows_plot_bitmap,
	.flush = flush,
	.path = path,
	.option_knockout = true,
};
