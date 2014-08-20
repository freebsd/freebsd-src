/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

/** \file
 * Target independent plotting (BeOS/Haiku implementation).
 */

#define __STDBOOL_H__	1
#include <math.h>
#include <BeBuild.h>
#include <Bitmap.h>
#include <GraphicsDefs.h>
#include <Region.h>
#include <View.h>
#include <Shape.h>
extern "C" {
#include "desktop/plotters.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/nsoption.h"
}
#include "beos/font.h"
#include "beos/gui.h"
#include "beos/plotters.h"
//#include "beos/scaffolding.h"
//#include "beos/options.h"
#include "beos/bitmap.h"

#warning MAKE ME static
/*static*/ BView *current_view;

/*
 * NOTE: BeOS rects differ from NetSurf ones:
 * the right-bottom pixel is actually part of the BRect!
 */

static bool nsbeos_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style);
static bool nsbeos_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *style);
static bool nsbeos_plot_polygon(const int *p, unsigned int n, const plot_style_t *style);
static bool nsbeos_plot_path(const float *p, unsigned int n, colour fill, float width,
                    colour c, const float transform[6]);
static bool nsbeos_plot_clip(const struct rect *ns_clip);
static bool nsbeos_plot_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle);
static bool nsbeos_plot_disc(int x, int y, int radius, const plot_style_t *style);
static bool nsbeos_plot_arc(int x, int y, int radius, int angle1, int angle2,
    		const plot_style_t *style);
static bool nsbeos_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags);


#warning make patterns nicer
static const pattern kDottedPattern = { 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa };
static const pattern kDashedPattern = { 0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0x33, 0x33 };

static const rgb_color kBlackColor = { 0, 0, 0, 255 };

struct plotter_table plot;

const struct plotter_table nsbeos_plotters = {
	nsbeos_plot_clip,
	nsbeos_plot_arc,
	nsbeos_plot_disc,
	nsbeos_plot_line,
	nsbeos_plot_rectangle,
	nsbeos_plot_polygon,
	nsbeos_plot_path,
	nsbeos_plot_bitmap,
	nsbeos_plot_text,
	NULL, // Group Start
	NULL, // Group End
	NULL, // Flush
	true // option_knockout
};


// #pragma mark - implementation


BView *nsbeos_current_gc(void)
{
	return current_view;
}

BView *nsbeos_current_gc_lock(void)
{
	BView *view = current_view;
	if (view && view->LockLooper())
		return view;
	return NULL;
}

void nsbeos_current_gc_unlock(void)
{
	if (current_view)
		current_view->UnlockLooper();
}

void nsbeos_current_gc_set(BView *view)
{
	// XXX: (un)lock previous ?
	current_view = view;
}

bool nsbeos_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	if (style->fill_type != PLOT_OP_TYPE_NONE) { 
		BView *view;

		view = nsbeos_current_gc/*_lock*/();
		if (view == NULL) {
			warn_user("No GC", 0);
			return false;
		}

		nsbeos_set_colour(style->fill_colour);

		BRect rect(x0, y0, x1 - 1, y1 - 1);
		view->FillRect(rect);

		//nsbeos_current_gc_unlock();

	}

        if (style->stroke_type != PLOT_OP_TYPE_NONE) { 
		pattern pat; 
		BView *view;

                switch (style->stroke_type) {
                case PLOT_OP_TYPE_SOLID: /**< Solid colour */
                default:
                        pat = B_SOLID_HIGH;
                        break;

                case PLOT_OP_TYPE_DOT: /**< Doted plot */
			pat = kDottedPattern;
                        break;

                case PLOT_OP_TYPE_DASH: /**< dashed plot */
			pat = kDashedPattern;
                        break;
                }

		view = nsbeos_current_gc/*_lock*/();
		if (view == NULL) {
			warn_user("No GC", 0);
			return false;
		}

		nsbeos_set_colour(style->stroke_colour);

		float pensize = view->PenSize();
		view->SetPenSize(style->stroke_width);

		BRect rect(x0, y0, x1, y1);
		view->StrokeRect(rect, pat);

		view->SetPenSize(pensize);

		//nsbeos_current_gc_unlock();

	}

	return true;
}



bool nsbeos_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *style)
{
	pattern pat;
	BView *view;

	switch (style->stroke_type) {
	case PLOT_OP_TYPE_SOLID: /**< Solid colour */
	default:
		pat = B_SOLID_HIGH;
		break;

	case PLOT_OP_TYPE_DOT: /**< Doted plot */
		pat = kDottedPattern;
		break;

	case PLOT_OP_TYPE_DASH: /**< dashed plot */
		pat = kDashedPattern;
		break;
	}

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(style->stroke_colour);

	float pensize = view->PenSize();
	view->SetPenSize(style->stroke_width);

	BPoint start(x0, y0);
	BPoint end(x1, y1);
	view->StrokeLine(start, end, pat);

	view->SetPenSize(pensize);

	//nsbeos_current_gc_unlock();

	return true;
}


bool nsbeos_plot_polygon(const int *p, unsigned int n, const plot_style_t *style)
{
	unsigned int i;
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(style->fill_colour);

	BPoint points[n];
	
	for (i = 0; i < n; i++) {
		points[i] = BPoint(p[2 * i] - 0.5, p[2 * i + 1] - 0.5);
	}

	if (style->fill_colour == NS_TRANSPARENT)
		view->StrokePolygon(points, (int32)n);
	else
		view->FillPolygon(points, (int32)n);

	return true;
}




bool nsbeos_plot_clip(const struct rect *ns_clip)
{
	BView *view;
	//fprintf(stderr, "%s(%d, %d, %d, %d)\n", __FUNCTION__, clip_x0, clip_y0, clip_x1, clip_y1);

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	BRect rect(ns_clip->x0, ns_clip->y0, ns_clip->x1 - 1,
			ns_clip->y1 - 1);
	BRegion clip(rect);
	view->ConstrainClippingRegion(NULL);
	if (view->Bounds() != rect)
		view->ConstrainClippingRegion(&clip);
		

	//nsbeos_current_gc_unlock();

	return true;
}


bool nsbeos_plot_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle)
{
	return nsfont_paint(fstyle, text, length, x, y);
}


bool nsbeos_plot_disc(int x, int y, int radius, const plot_style_t *style)
{
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(style->fill_colour);

	BPoint center(x, y);
	if (style->fill_type != PLOT_OP_TYPE_NONE)
		view->FillEllipse(center, radius, radius);
	else
		view->StrokeEllipse(center, radius, radius);

	//nsbeos_current_gc_unlock();

	return true;
}

bool nsbeos_plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *style)
{
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	nsbeos_set_colour(style->fill_colour);

	BPoint center(x, y);
	float angle = angle1; // in degree
	float span = angle2 - angle1; // in degree
	view->StrokeArc(center, radius, radius, angle, span);

	//nsbeos_current_gc_unlock();

	return true;
}

static bool nsbeos_plot_bbitmap(int x, int y, int width, int height,
                              BBitmap *b, colour bg)
{
	/* XXX: This currently ignores the background colour supplied.
	 * Does this matter?
	 */

	if (width == 0 || height == 0)
		return true;

	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	drawing_mode oldmode = view->DrawingMode();
	source_alpha alpha;
	alpha_function func;
	view->GetBlendingMode(&alpha, &func);
	//view->SetDrawingMode(B_OP_OVER);
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	// XXX DrawBitmap() resamples if rect doesn't match,
	// but doesn't do any filtering
	// XXX: use Zeta API if available ?

	BRect rect(x, y, x + width - 1, y + height - 1);
	/*
	rgb_color old = view->LowColor();
	if (bg != NS_TRANSPARENT) {
		view->SetLowColor(nsbeos_rgb_colour(bg));
		view->FillRect(rect, B_SOLID_LOW);
	}
	*/
	view->DrawBitmap(b, rect);
	// maybe not needed?
	//view->SetLowColor(old);
	view->SetBlendingMode(alpha, func);
	view->SetDrawingMode(oldmode);

	//nsbeos_current_gc_unlock();

	return true;
}


bool nsbeos_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags)
{
	int doneheight = 0, donewidth = 0;
	BBitmap *primary;
	BBitmap *pretiled;
        bool repeat_x = (flags & BITMAPF_REPEAT_X);
        bool repeat_y = (flags & BITMAPF_REPEAT_Y);

	if (!(repeat_x || repeat_y)) {
		/* Not repeating at all, so just plot it */
                primary = nsbeos_bitmap_get_primary(bitmap);
                return nsbeos_plot_bbitmap(x, y, width, height, primary, bg);
	}

	if (repeat_x && !repeat_y)
		pretiled = nsbeos_bitmap_get_pretile_x(bitmap);
	if (repeat_x && repeat_y)
		pretiled = nsbeos_bitmap_get_pretile_xy(bitmap);
	if (!repeat_x && repeat_y)
		pretiled = nsbeos_bitmap_get_pretile_y(bitmap);
	primary = nsbeos_bitmap_get_primary(bitmap);

	/* use the primary and pretiled widths to scale the w/h provided */
	width *= pretiled->Bounds().Width() + 1;
	width /= primary->Bounds().Width() + 1;
	height *= pretiled->Bounds().Height() + 1;
	height /= primary->Bounds().Height() + 1;

	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	// XXX: do we really need to use clipping reg ?
	// I guess it's faster to not draw clipped out stuff...

	BRect cliprect;
	BRegion clipreg;
	view->GetClippingRegion(&clipreg);
	cliprect = clipreg.Frame();

	//XXX: FIXME

	if (y > cliprect.top)
		doneheight = ((int)cliprect.top - height) + ((y - (int)cliprect.top) % height);
	else
		doneheight = y;

	while (doneheight < ((int)cliprect.bottom)) {
		if (x > cliprect.left)
			donewidth = ((int)cliprect.left - width) + ((x - (int)cliprect.left) % width);
		else
			donewidth = x;
		while (donewidth < (cliprect.right)) {
			nsbeos_plot_bbitmap(donewidth, doneheight,
					  width, height, pretiled, bg);
			donewidth += width;
			if (!repeat_x) break;
		}
		doneheight += height;
		if (!repeat_y) break;
	}

#warning WRITEME
	return true;
}

static BPoint transform_pt(float x, float y, const float transform[6])
{
#warning XXX: verify
	//return BPoint(x, y);
	BPoint pt;
	pt.x = x * transform[0] + y * transform[1] + transform[4];
	pt.y = x * transform[2] + y * transform[3] + transform[5];
	/*
	printf("TR: {%f, %f} { %f, %f, %f, %f, %f, %f} = { %f, %f }\n",
		x, y,
		transform[0], transform[1], transform[2],
		transform[3], transform[4], transform[5],
		pt.x, pt.y);
	*/
	return pt;
}

bool nsbeos_plot_path(const float *p, unsigned int n, colour fill, float width,
                colour c, const float transform[6])
{
	unsigned int i;

	if (n == 0)
		return true;

	if (p[0] != PLOTTER_PATH_MOVE) {
		LOG(("path doesn't start with a move"));
		return false;
	}

	BShape shape;

	for (i = 0; i < n; ) {
		if (p[i] == PLOTTER_PATH_MOVE) {
			BPoint pt(transform_pt(p[i + 1], p[i + 2], transform));
			shape.MoveTo(pt);
			i += 3;
		} else if (p[i] == PLOTTER_PATH_CLOSE) {
			shape.Close();
			i++;
		} else if (p[i] == PLOTTER_PATH_LINE) {
			BPoint pt(transform_pt(p[i + 1], p[i + 2], transform));
			shape.LineTo(pt);
			i += 3;
		} else if (p[i] == PLOTTER_PATH_BEZIER) {
			BPoint pt[3] = {
				transform_pt(p[i + 1], p[i + 2], transform),
				transform_pt(p[i + 3], p[i + 4], transform),
				transform_pt(p[i + 5], p[i + 6], transform)
			};
			shape.BezierTo(pt);
			i += 7;
		} else {
			LOG(("bad path command %f", p[i]));
			return false;
		}
	}
	shape.Close();

	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL)
		return false;

	rgb_color old_high = view->HighColor();
	float old_pen = view->PenSize();
	view->SetPenSize(width);
	view->MovePenTo(0, 0);
	if (fill != NS_TRANSPARENT) {
		view->SetHighColor(nsbeos_rgb_colour(fill));
		view->FillShape(&shape);
	}
	if (c != NS_TRANSPARENT) {
		view->SetHighColor(nsbeos_rgb_colour(c));
		view->StrokeShape(&shape);
	}
	// restore
	view->SetPenSize(old_pen);
	view->SetHighColor(old_high);

	//nsbeos_current_gc_unlock();

	return true;
}

rgb_color nsbeos_rgb_colour(colour c)
{
	rgb_color color;
	if (c == NS_TRANSPARENT)
		return B_TRANSPARENT_32_BIT;
	color.red = c & 0x0000ff;
	color.green = (c & 0x00ff00) >> 8;
	color.blue = (c & 0xff0000) >> 16;
	return color;
}

void nsbeos_set_colour(colour c)
{
	rgb_color color = nsbeos_rgb_colour(c);
	BView *view = nsbeos_current_gc();
	view->SetHighColor(color);
}

/** Plot a caret.  It is assumed that the plotters have been set up. */
void nsbeos_plot_caret(int x, int y, int h)
{
	BView *view;

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL)
		/* TODO: report an error here */
		return;

	BPoint start(x, y);
	BPoint end(x, y + h - 1);
#if defined(__HAIKU__) || defined(B_BEOS_VERSION_DANO)
	view->SetHighColor(ui_color(B_DOCUMENT_TEXT_COLOR));
#else
	view->SetHighColor(kBlackColor);
#endif
	view->StrokeLine(start, end);

	//nsbeos_current_gc_unlock();

}

#ifdef TEST_PLOTTERS
//
static void test_plotters(void)
{
	int x0, y0;
	int x1, y1;
	struct rect r;

	x0 = 5;
	y0 = 5;
	x1 = 35;
	y1 = 6;
	
	plot.line(x0, y0, x1, y1, 1, 0x0000ff00, false, false);
	y0+=2; y1+=2;
	plot.line(x0, y0, x1, y1, 1, 0x0000ff00, true, false);
	y0+=2; y1+=2;
	plot.line(x0, y0, x1, y1, 1, 0x0000ff00, false, true);
	y0+=2; y1+=2;
	plot.line(x0, y0, x1, y1, 1, 0x0000ff00, true, true);
	y0+=10; y1+=20;
	
	plot.fill(x0, y0, x1, y1, 0x00ff0000);
	plot.rectangle(x0+10, y0+10, x1-x0+1, y1-y0+1, 2, 0x00ffff00, true, false);
	y0+=30; y1+=30;

	r.x0 = x0 + 2;
	r.y0 = y0 + 2;
	r.x1 = x1 - 2;
	r.y1 = y1 - 2;
	plot.clip(&r);

	plot.fill(x0, y0, x1, y1, 0x00000000);
	plot.disc(x1, y1, 8, 0x000000ff, false);

	r.x0 = 0;
	r.y0 = 0;
	r.x1 = 300;
	r.y1 = 300;
	plot.clip(&r);

	y0+=30; y1+=30;
	
}

#include <Application.h>
#include <View.h>
#include <Window.h>
class PTView : public BView {
public:
	PTView(BRect frame) : BView(frame, "view", B_FOLLOW_NONE, B_WILL_DRAW) {};
	virtual ~PTView() {};
	virtual void Draw(BRect update)
	{
		test_plotters();
	};

};

extern "C" void test_plotters_main(void);
void test_plotters_main(void)
{
	BApplication app("application/x-vnd.NetSurf");
	memcpy(&plot, &nsbeos_plotters, sizeof(plot));
	BRect frame(0,0,300,300);
	PTView *view = new PTView(frame);
	frame.OffsetBySelf(100,100);
	BWindow *win = new BWindow(frame, "NetSurfPlotterTest", B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE);
	win->AddChild(view);
	nsbeos_current_gc_set(view);
	win->Show();
	app.Run();
}
#endif /* TEST_PLOTTERS */

