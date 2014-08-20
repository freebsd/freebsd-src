/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
 * Copyright 2009 John Tytgat <joty@netsurf-browser.org>
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
 * Target independent PDF plotting using Haru Free PDF Library.
 */

#include "utils/config.h"
#ifdef WITH_PDF_EXPORT

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <hpdf.h>

#include "content/hlcache.h"
#include "utils/nsoption.h"
#include "desktop/plotters.h"
#include "desktop/print.h"
#include "desktop/printer.h"
#include "desktop/save_pdf/pdf_plotters.h"
#include "image/bitmap.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/useragent.h"

#include "font_haru.h"

/* #define PDF_DEBUG */
/* #define PDF_DEBUG_DUMPGRID */

static bool pdf_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *style);
static bool pdf_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *pstyle);
static bool pdf_plot_polygon(const int *p, unsigned int n, const plot_style_t *style);
static bool pdf_plot_clip(const struct rect *clip);
static bool pdf_plot_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle);
static bool pdf_plot_disc(int x, int y, int radius, const plot_style_t *style);
static bool pdf_plot_arc(int x, int y, int radius, int angle1, int angle2,
    		const plot_style_t *style);
static bool pdf_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags);
static bool pdf_plot_path(const float *p, unsigned int n, colour fill, float width,
		colour c, const float transform[6]);

static HPDF_Image pdf_extract_image(struct bitmap *bitmap);

static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no,
		void *user_data);

#ifdef PDF_DEBUG_DUMPGRID
static void pdf_plot_grid(int x_dist,int y_dist,unsigned int colour);
#endif

typedef enum {
	DashPattern_eNone,
	DashPattern_eDash,
	DashPattern_eDotted
} DashPattern_e;

/* Wrapper routines to minimize gstate updates in the produced PDF file.  */
static void pdfw_gs_init(void);
static void pdfw_gs_save(HPDF_Page page);
static void pdfw_gs_restore(HPDF_Page page);
static void pdfw_gs_fillcolour(HPDF_Page page, colour col);
static void pdfw_gs_strokecolour(HPDF_Page page, colour col);
static void pdfw_gs_linewidth(HPDF_Page page, float lineWidth);
static void pdfw_gs_font(HPDF_Page page, HPDF_Font font, HPDF_REAL font_size);
static void pdfw_gs_dash(HPDF_Page page, DashPattern_e dash);

/** 
 * Our PDF gstate mirror which we use to minimize gstate updates
 * in the PDF file.
 */
typedef struct {
	colour fillColour; /**< Current fill colour.  */
	colour strokeColour; /**< Current stroke colour.  */
	float lineWidth; /**< Current line width.  */
	HPDF_Font font; /**< Current font.  */
	HPDF_REAL font_size; /**< Current font size.  */
	DashPattern_e dash; /**< Current dash state.  */
} PDFW_GState;

static void apply_clip_and_mode(bool selectTextMode, colour fillCol,
	colour strokeCol, float lineWidth, DashPattern_e dash);

#define PDFW_MAX_GSTATES 4
static PDFW_GState pdfw_gs[PDFW_MAX_GSTATES];
static unsigned int pdfw_gs_level;

static HPDF_Doc pdf_doc; /**< Current PDF document.  */
static HPDF_Page pdf_page; /**< Current page.  */

/*PDF Page size*/
static HPDF_REAL page_height, page_width;

static bool in_text_mode; /**< true if we're currently in text mode or not.  */
static bool clip_update_needed; /**< true if pdf_plot_clip was invoked for
	current page and not yet synced with PDF output.  */
static int last_clip_x0, last_clip_y0, last_clip_x1, last_clip_y1;

static const struct print_settings *settings;

static const struct plotter_table pdf_plotters = {
	.rectangle = pdf_plot_rectangle,
	.line = pdf_plot_line,
	.polygon = pdf_plot_polygon,
	.clip = pdf_plot_clip,
	.text = pdf_plot_text,
	.disc = pdf_plot_disc,
	.arc = pdf_plot_arc,
	.bitmap = pdf_plot_bitmap_tile,
	.path = pdf_plot_path,
	.option_knockout = false,
};

const struct printer pdf_printer = {
	&pdf_plotters,
	pdf_begin,
	pdf_next_page,
	pdf_end
};

static char *owner_pass;
static char *user_pass;

bool pdf_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *pstyle)
{
	DashPattern_e dash;
#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %f %X", x0, y0, x1, y1, page_height - y0, pstyle->fill_colour));
#endif

	if (pstyle->fill_type != PLOT_OP_TYPE_NONE) {

		apply_clip_and_mode(false, pstyle->fill_colour, NS_TRANSPARENT, 0., DashPattern_eNone);

		/* Normalize boundaries of the area - to prevent
		   overflows.  It is needed only in a few functions,
		   where integers are subtracted.  When the whole
		   browser window is meant min and max int values are
		   used what must be handled in paged output.
		*/
		x0 = min(max(x0, 0), page_width);
		y0 = min(max(y0, 0), page_height);
		x1 = min(max(x1, 0), page_width);
		y1 = min(max(y1, 0), page_height);

		HPDF_Page_Rectangle(pdf_page, x0, page_height - y1, x1 - x0, y1 - y0);
		HPDF_Page_Fill(pdf_page);

	}

	if (pstyle->stroke_type != PLOT_OP_TYPE_NONE) {

		switch (pstyle->stroke_type) {
		case PLOT_OP_TYPE_DOT:
			dash = DashPattern_eDotted;
			break;

		case PLOT_OP_TYPE_DASH:
			dash = DashPattern_eDash;
			break;

		default:
			dash = DashPattern_eNone;
			break;

		}

		apply_clip_and_mode(false, 
				    NS_TRANSPARENT, 
				    pstyle->stroke_colour, 
				    pstyle->stroke_width,
				    dash);

		HPDF_Page_Rectangle(pdf_page, x0, page_height - y0, x1 - x0, -(y1 - y0));
		HPDF_Page_Stroke(pdf_page);
	}

	return true;
}

bool pdf_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *pstyle)
{
	DashPattern_e dash;

	switch (pstyle->stroke_type) {
	case PLOT_OP_TYPE_DOT:
		dash = DashPattern_eDotted;
		break;

	case PLOT_OP_TYPE_DASH:
		dash = DashPattern_eDash;
		break;

	default:
		dash = DashPattern_eNone;
		break;

	}

	apply_clip_and_mode(false, 
			    NS_TRANSPARENT, 
			    pstyle->stroke_colour, 
			    pstyle->stroke_width,
			    dash);

	HPDF_Page_MoveTo(pdf_page, x0, page_height - y0);
	HPDF_Page_LineTo(pdf_page, x1, page_height - y1);
	HPDF_Page_Stroke(pdf_page);

	return true;
}

bool pdf_plot_polygon(const int *p, unsigned int n, const plot_style_t *style)
{
	unsigned int i;
#ifdef PDF_DEBUG
	int pmaxx = p[0], pmaxy = p[1];
	int pminx = p[0], pminy = p[1];
	LOG(("."));
#endif
	if (n == 0)
		return true;

	apply_clip_and_mode(false, style->fill_colour, NS_TRANSPARENT, 0., DashPattern_eNone);

	HPDF_Page_MoveTo(pdf_page, p[0], page_height - p[1]);
	for (i = 1 ; i<n ; i++) {
		HPDF_Page_LineTo(pdf_page, p[i*2], page_height - p[i*2+1]);
#ifdef PDF_DEBUG
		pmaxx = max(pmaxx, p[i*2]);
		pmaxy = max(pmaxy, p[i*2+1]);
		pminx = min(pminx, p[i*2]);
		pminy = min(pminy, p[i*2+1]);
#endif
	}

#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %f", pminx, pminy, pmaxx, pmaxy, page_height - pminy));
#endif

	HPDF_Page_Fill(pdf_page);

	return true;
}


/**here the clip is only queried */
bool pdf_plot_clip(const struct rect *clip)
{
#ifdef PDF_DEBUG
	LOG(("%d %d %d %d", clip->x0, clip->y0, clip->x1, clip->y1));
#endif

	/*Normalize cllipping area - to prevent overflows.
	  See comment in pdf_plot_fill.
	*/
	last_clip_x0 = min(max(clip->x0, 0), page_width);
	last_clip_y0 = min(max(clip->y0, 0), page_height);
	last_clip_x1 = min(max(clip->x1, 0), page_width);
	last_clip_y1 = min(max(clip->y1, 0), page_height);

	clip_update_needed = true;

	return true;
}

bool pdf_plot_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle)
{
#ifdef PDF_DEBUG
	LOG((". %d %d %.*s", x, y, (int)length, text));
#endif
	char *word;
	HPDF_Font pdf_font;
	HPDF_REAL size;

	if (length == 0)
		return true;

	apply_clip_and_mode(true, fstyle->foreground, NS_TRANSPARENT, 0., 
			DashPattern_eNone);

	haru_nsfont_apply_style(fstyle, pdf_doc, pdf_page, &pdf_font, &size);
	pdfw_gs_font(pdf_page, pdf_font, size);

	/* FIXME: UTF-8 to current font encoding needs to done.  Or the font
	 * encoding needs to be UTF-8 or other Unicode encoding.  */
	word = (char *)malloc( sizeof(char) * (length+1) );
	if (word == NULL)
		return false;
	memcpy(word, text, length);
	word[length] = '\0';

	HPDF_Page_TextOut (pdf_page, x, page_height - y, word);

	free(word);

	return true;
}

bool pdf_plot_disc(int x, int y, int radius, const plot_style_t *style)
{
#ifdef PDF_DEBUG
	LOG(("."));
#endif
	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		apply_clip_and_mode(false,
				    style->fill_colour, 
				    NS_TRANSPARENT,
				    1., DashPattern_eNone);

		HPDF_Page_Circle(pdf_page, x, page_height - y, radius);

		HPDF_Page_Fill(pdf_page);
	}

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		/* FIXME: line width 1 is ok ? */
		apply_clip_and_mode(false,
				    NS_TRANSPARENT, 
				    style->stroke_colour,
				    1., DashPattern_eNone);

		HPDF_Page_Circle(pdf_page, x, page_height - y, radius);

		HPDF_Page_Stroke(pdf_page);
	}

	return true;
}

bool pdf_plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *style)
{
#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %d %X", x, y, radius, angle1, angle2, style->stroke_colour));
#endif

	/* FIXME: line width 1 is ok ? */
	apply_clip_and_mode(false, NS_TRANSPARENT, style->fill_colour, 1., DashPattern_eNone);

	/* Normalize angles */
	angle1 %= 360;
	angle2 %= 360;
	if (angle1 > angle2)
		angle1 -= 360;

	HPDF_Page_Arc(pdf_page, x, page_height - y, radius, angle1, angle2);

	HPDF_Page_Stroke(pdf_page);
	return true;
}


bool pdf_plot_bitmap_tile(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
  		bitmap_flags_t flags)
{
	HPDF_Image image;
	HPDF_REAL current_x, current_y ;
	HPDF_REAL max_width, max_height;

#ifdef PDF_DEBUG
	LOG(("%d %d %d %d %p 0x%x", x, y, width, height,
	     bitmap, bg));
#endif
 	if (width == 0 || height == 0)
 		return true;

	apply_clip_and_mode(false, NS_TRANSPARENT, NS_TRANSPARENT, 0., DashPattern_eNone);

	image = pdf_extract_image(bitmap);
	if (!image)
		return false;

	/*The position of the next tile*/
	max_width =  (flags & BITMAPF_REPEAT_X) ? page_width : width;
	max_height = (flags & BITMAPF_REPEAT_Y) ? page_height : height;

	for (current_y = 0; current_y < max_height; current_y += height)
		for (current_x = 0; current_x < max_width; current_x += width)
			HPDF_Page_DrawImage(pdf_page, image,
					current_x + x,
					page_height - current_y - y - height,
					width, height);

	return true;
}

HPDF_Image pdf_extract_image(struct bitmap *bitmap)
{
	HPDF_Image image = NULL;
        hlcache_handle *content = NULL;

        /* TODO - get content from bitmap pointer */

	if (content) {
		const char *source_data;
		unsigned long source_size;

		/*Not sure if I don't have to check if downloading has been
		finished.
		Other way - lock pdf plotting while fetching a website
		*/
		source_data = content_get_source_data(content, &source_size);

		switch(content_get_type(content)){
		/*Handle "embeddable" types of images*/
		case CONTENT_JPEG:
 			image = HPDF_LoadJpegImageFromMem(pdf_doc,
 					(const HPDF_BYTE *) source_data,
 					source_size);
 			break;

		/*Disabled until HARU PNG support will be more stable.

		case CONTENT_PNG:
			image = HPDF_LoadPngImageFromMem(pdf_doc,
					(const HPDF_BYTE *)content->source_data,
					content->total_size);
			break;*/
		default:
			break;
		}
	}

	if (!image) {
		HPDF_Image smask;
		unsigned char *img_buffer, *rgb_buffer, *alpha_buffer;
		int img_width, img_height, img_rowstride;
		int i, j;

		/*Handle pixmaps*/
		img_buffer = bitmap_get_buffer(bitmap);
		img_width = bitmap_get_width(bitmap);
		img_height = bitmap_get_height(bitmap);
		img_rowstride = bitmap_get_rowstride(bitmap);

		rgb_buffer = (unsigned char *)malloc(3 * img_width * img_height);
		alpha_buffer = (unsigned char *)malloc(img_width * img_height);
		if (rgb_buffer == NULL || alpha_buffer == NULL) {
			LOG(("Not enough memory to create RGB buffer"));
			free(rgb_buffer);
			free(alpha_buffer);
			return NULL;
		}

		for (i = 0; i < img_height; i++)
			for (j = 0; j < img_width; j++) {
				rgb_buffer[((i * img_width) + j) * 3] =
				  img_buffer[(i * img_rowstride) + (j * 4)];

				rgb_buffer[(((i * img_width) + j) * 3) + 1] =
				  img_buffer[(i * img_rowstride) + (j * 4) + 1];

				rgb_buffer[(((i * img_width) + j) * 3) + 2] =
				  img_buffer[(i * img_rowstride) + (j * 4) + 2];

				alpha_buffer[(i * img_width)+j] =
				  img_buffer[(i * img_rowstride) + (j * 4) + 3];
			}

		smask = HPDF_LoadRawImageFromMem(pdf_doc, alpha_buffer,
				img_width, img_height,
     				HPDF_CS_DEVICE_GRAY, 8);

		image = HPDF_LoadRawImageFromMem(pdf_doc, rgb_buffer,
				img_width, img_height,
     				HPDF_CS_DEVICE_RGB, 8);

		if (HPDF_Image_AddSMask(image, smask) != HPDF_OK)
			image = NULL;

		free(rgb_buffer);
		free(alpha_buffer);
	}

	return image;
}

/**
 * Enter/leave text mode and update PDF gstate for its clip, fill & stroke
 * colour, line width and dash pattern parameters.
 * \param selectTextMode true if text mode needs to be entered if required;
 * false otherwise.
 * \param fillCol Desired fill colour, use NS_TRANSPARENT if no update is
 * required.
 * \param strokeCol Desired stroke colour, use NS_TRANSPARENT if no update is
 * required.
 * \param lineWidth Desired line width. Only taken into account when strokeCol
 * is different from NS_TRANSPARENT.
 * \param dash Desired dash pattern. Only taken into account when strokeCol
 * is different from NS_TRANSPARENT.
 */
static void apply_clip_and_mode(bool selectTextMode, colour fillCol,
		colour strokeCol, float lineWidth, DashPattern_e dash)
{
	/* Leave text mode when
	 *  1) we're not setting text anymore
	 *  2) or we need to update the current clippath
	 *  3) or we need to update any fill/stroke colour, linewidth or dash.
	 * Note: the test on stroke parameters (stroke colour, line width and
	 * dash) is commented out as if these need updating we want to be
	 * outside the text mode anyway (i.e. selectTextMode is false).
	 */
	if (in_text_mode && (!selectTextMode || clip_update_needed
		|| (fillCol != NS_TRANSPARENT
			&& fillCol != pdfw_gs[pdfw_gs_level].fillColour)
		/* || (strokeCol != NS_TRANSPARENT
			&& (strokeCol != pdfw_gs[pdfw_gs_level].strokeColour
				|| lineWidth != pdfw_gs[pdfw_gs_level].lineWidth
				|| dash != pdfw_gs[pdfw_gs_level].dash)) */)) {
		HPDF_Page_EndText(pdf_page);
		in_text_mode = false;
	}

	if (clip_update_needed)
		pdfw_gs_restore(pdf_page);

	/* Update fill/stroke colour, linewidth and dash when needed.  */
	if (fillCol != NS_TRANSPARENT)
		pdfw_gs_fillcolour(pdf_page, fillCol);
	if (strokeCol != NS_TRANSPARENT) {
		pdfw_gs_strokecolour(pdf_page, strokeCol);
		pdfw_gs_linewidth(pdf_page, lineWidth);
		pdfw_gs_dash(pdf_page, dash);
	}

	if (clip_update_needed) {
		pdfw_gs_save(pdf_page);

		HPDF_Page_Rectangle(pdf_page, last_clip_x0,
				page_height - last_clip_y1,
				last_clip_x1 - last_clip_x0,
				last_clip_y1 - last_clip_y0);
		HPDF_Page_Clip(pdf_page);
		HPDF_Page_EndPath(pdf_page);

		clip_update_needed = false;
	}

	if (selectTextMode && !in_text_mode) {
		HPDF_Page_BeginText(pdf_page);
		in_text_mode = true;
	}
}

static inline float transform_x(const float transform[6], float x, float y)
{
	return transform[0] * x + transform[2] * y + transform[4];
}

static inline float transform_y(const float transform[6], float x, float y)
{
	return page_height
		- (transform[1] * x + transform[3] * y + transform[5]);
}

bool pdf_plot_path(const float *p, unsigned int n, colour fill, float width,
		colour c, const float transform[6])
{
	unsigned int i;
	bool empty_path;

#ifdef PDF_DEBUG
	LOG(("."));
#endif

	if (n == 0)
		return true;

	if (c == NS_TRANSPARENT && fill == NS_TRANSPARENT)
		return true;

	if (p[0] != PLOTTER_PATH_MOVE)
		return false;

	apply_clip_and_mode(false, fill, c, width, DashPattern_eNone);

	empty_path = true;
	for (i = 0 ; i < n ; ) {
		if (p[i] == PLOTTER_PATH_MOVE) {
			HPDF_Page_MoveTo(pdf_page,
					transform_x(transform, p[i+1], p[i+2]),
					transform_y(transform, p[i+1], p[i+2]));
			i+= 3;
		} else if (p[i] == PLOTTER_PATH_CLOSE) {
			if (!empty_path)
				HPDF_Page_ClosePath(pdf_page);
			i++;
		} else if (p[i] == PLOTTER_PATH_LINE) {
			HPDF_Page_LineTo(pdf_page,
					transform_x(transform, p[i+1], p[i+2]),
					transform_y(transform, p[i+1], p[i+2]));
			i+=3;
			empty_path = false;
		} else if (p[i] == PLOTTER_PATH_BEZIER) {
			HPDF_Page_CurveTo(pdf_page,
					transform_x(transform, p[i+1], p[i+2]),
					transform_y(transform, p[i+1], p[i+2]),
					transform_x(transform, p[i+3], p[i+4]),
					transform_y(transform, p[i+3], p[i+4]),
					transform_x(transform, p[i+5], p[i+6]),
					transform_y(transform, p[i+5], p[i+6]));
			i += 7;
			empty_path = false;
		} else {
			LOG(("bad path command %f", p[i]));
			return false;
		}
	}

	if (empty_path) {
		HPDF_Page_EndPath(pdf_page);
		return true;
	}

	if (fill != NS_TRANSPARENT) {
		if (c != NS_TRANSPARENT)
			HPDF_Page_FillStroke(pdf_page);
		else
			HPDF_Page_Fill(pdf_page);
	}
	else
		HPDF_Page_Stroke(pdf_page);

	return true;
}

/**
 * Begin pdf plotting - initialize a new document
 * \param path Output file path
 * \param pg_width page width
 * \param pg_height page height
 */
bool pdf_begin(struct print_settings *print_settings)
{
	pdfw_gs_init();

	if (pdf_doc != NULL)
		HPDF_Free(pdf_doc);
	pdf_doc = HPDF_New(error_handler, NULL);
	if (!pdf_doc) {
		LOG(("Error creating pdf_doc"));
		return false;
	}

	settings = print_settings;

	page_width = settings->page_width - 
			FIXTOFLT(FSUB(settings->margins[MARGINLEFT],
			settings->margins[MARGINRIGHT]));

	page_height = settings->page_height - 
			FIXTOFLT(settings->margins[MARGINTOP]);


#ifndef PDF_DEBUG
	if (option_enable_PDF_compression)
		HPDF_SetCompressionMode(pdf_doc, HPDF_COMP_ALL); /*Compression on*/
#endif
	HPDF_SetInfoAttr(pdf_doc, HPDF_INFO_CREATOR, user_agent_string());

	pdf_page = NULL;

#ifdef PDF_DEBUG
	LOG(("pdf_begin finishes"));
#endif
	return true;
}


bool pdf_next_page(void)
{
#ifdef PDF_DEBUG
	LOG(("pdf_next_page begins"));
#endif
	clip_update_needed = false;
	if (pdf_page != NULL) {
		apply_clip_and_mode(false, NS_TRANSPARENT, NS_TRANSPARENT, 0.,
				DashPattern_eNone);
		pdfw_gs_restore(pdf_page);
	}

#ifdef PDF_DEBUG_DUMPGRID
	if (pdf_page != NULL) {
		pdf_plot_grid(10, 10, 0xCCCCCC);
		pdf_plot_grid(100, 100, 0xCCCCFF);
	}
#endif
	pdf_page = HPDF_AddPage(pdf_doc);
	if (pdf_page == NULL)
		return false;

	HPDF_Page_SetWidth (pdf_page, settings->page_width);
	HPDF_Page_SetHeight(pdf_page, settings->page_height);

	HPDF_Page_Concat(pdf_page, 1, 0, 0, 1, 
			FIXTOFLT(settings->margins[MARGINLEFT]), 0);

	pdfw_gs_save(pdf_page);

#ifdef PDF_DEBUG
	LOG(("%f %f", page_width, page_height));
#endif

	return true;
}


void pdf_end(void)
{
#ifdef PDF_DEBUG
	LOG(("pdf_end begins"));
#endif
	clip_update_needed = false;
	if (pdf_page != NULL) {
		apply_clip_and_mode(false, NS_TRANSPARENT, NS_TRANSPARENT, 0.,
				DashPattern_eNone);
		pdfw_gs_restore(pdf_page);
	}

#ifdef PDF_DEBUG_DUMPGRID
	if (pdf_page != NULL) {
		pdf_plot_grid(10, 10, 0xCCCCCC);
		pdf_plot_grid(100, 100, 0xCCCCFF);
	}
#endif

	assert(settings->output != NULL);

	/*Encryption on*/
	if (option_enable_PDF_password)
		PDF_Password(&owner_pass, &user_pass,
				(void *)settings->output);
	else
		save_pdf(settings->output);
#ifdef PDF_DEBUG
	LOG(("pdf_end finishes"));
#endif
}

/** saves the pdf optionally encrypting it before*/
void save_pdf(const char *path)
{
	bool success = false;

	if (option_enable_PDF_password && owner_pass != NULL ) {
		HPDF_SetPassword(pdf_doc, owner_pass, user_pass);
		HPDF_SetEncryptionMode(pdf_doc, HPDF_ENCRYPT_R3, 16);
		free(owner_pass);
		free(user_pass);
	}

	if (path != NULL) {
		if (HPDF_SaveToFile(pdf_doc, path) != HPDF_OK)
			remove(path);
		else
			success = true;
	}

	if (!success)
		warn_user("Unable to save PDF file.", 0);

	HPDF_Free(pdf_doc);
	pdf_doc = NULL;
}


/**
 * Haru error handler
 * for debugging purposes - it immediately exits the program on the first error,
 * as it would otherwise flood the user with all resulting complications,
 * covering the most important error source.
*/
static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no,
		void *user_data)
{
	LOG(("ERROR:\n\terror_no=%x\n\tdetail_no=%d\n",	(HPDF_UINT)error_no,
			(HPDF_UINT)detail_no));
#ifdef PDF_DEBUG
	exit(1);
#endif
}

/**
 * This function plots a grid - used for debug purposes to check if all
 * elements' final coordinates are correct.
*/
#ifdef PDF_DEBUG_DUMPGRID
void pdf_plot_grid(int x_dist, int y_dist, unsigned int colour)
{
	for (int i = x_dist ; i < page_width ; i += x_dist)
		pdf_plot_line(i, 0, i, page_height, 1, colour, false, false);

	for (int i = y_dist ; i < page_height ; i += x_dist)
		pdf_plot_line(0, i, page_width, i, 1, colour, false, false);
}
#endif

/**
 * Initialize the gstate wrapper code.
 */
void pdfw_gs_init()
{
	pdfw_gs_level = 0;
	pdfw_gs[0].fillColour = 0x00000000; /* Default PDF fill colour is black.  */
	pdfw_gs[0].strokeColour = 0x00000000; /* Default PDF stroke colour is black.  */
	pdfw_gs[0].lineWidth = 1.0; /* Default PDF line width is 1.  */
	pdfw_gs[0].font = NULL;
	pdfw_gs[0].font_size = 0.;
	pdfw_gs[0].dash = DashPattern_eNone; /* Default dash state is a solid line.  */
}

/**
 * Increase gstate level.
 * \param page	PDF page where the update needs to happen.
 */
void pdfw_gs_save(HPDF_Page page)
{
	if (pdfw_gs_level == PDFW_MAX_GSTATES)
		abort();
	pdfw_gs[pdfw_gs_level + 1] = pdfw_gs[pdfw_gs_level];
	++pdfw_gs_level;
	HPDF_Page_GSave(page);
}

/**
 * Decrease gstate level and restore the gstate to its value at last save
 * operation.
 * \param page	PDF page where the update needs to happen.
 */
void pdfw_gs_restore(HPDF_Page page)
{
	if (pdfw_gs_level == 0)
		abort();
	--pdfw_gs_level;
	HPDF_Page_GRestore(page);
}

#define RBYTE(x) (((x) & 0x0000FF) >>  0)
#define GBYTE(x) (((x) & 0x00FF00) >>  8)
#define BBYTE(x) (((x) & 0xFF0000) >> 16)
#define R(x) (RBYTE(x) / 255.)
#define G(x) (GBYTE(x) / 255.)
#define B(x) (BBYTE(x) / 255.)

/**
 * Checks if given fill colour is already set in PDF gstate and if not,
 * update the gstate accordingly.
 * \param page	PDF page where the update needs to happen.
 * \param col	Wanted fill colour.
 */
void pdfw_gs_fillcolour(HPDF_Page page, colour col)
{
	if (col == pdfw_gs[pdfw_gs_level].fillColour)
		return;
	pdfw_gs[pdfw_gs_level].fillColour = col;
	if (RBYTE(col) == GBYTE(col) && GBYTE(col) == BBYTE(col))
		HPDF_Page_SetGrayFill(pdf_page, R(col));
	else
		HPDF_Page_SetRGBFill(pdf_page, R(col), G(col), B(col));
}

/**
 * Checks if given stroke colour is already set in PDF gstate and if not,
 * update the gstate accordingly.
 * \param page	PDF page where the update needs to happen.
 * \param col	Wanted stroke colour.
 */
void pdfw_gs_strokecolour(HPDF_Page page, colour col)
{
	if (col == pdfw_gs[pdfw_gs_level].strokeColour)
		return;
	pdfw_gs[pdfw_gs_level].strokeColour = col;
	if (RBYTE(col) == GBYTE(col) && GBYTE(col) == BBYTE(col))
		HPDF_Page_SetGrayStroke(pdf_page, R(col));
	else
		HPDF_Page_SetRGBStroke(pdf_page, R(col), G(col), B(col));
}

/**
 * Checks if given line width is already set in PDF gstate and if not, update
 * the gstate accordingly.
 * \param page		PDF page where the update needs to happen.
 * \param lineWidth	Wanted line width.
 */
void pdfw_gs_linewidth(HPDF_Page page, float lineWidth)
{
	if (lineWidth == pdfw_gs[pdfw_gs_level].lineWidth)
		return;
	pdfw_gs[pdfw_gs_level].lineWidth = lineWidth;
	HPDF_Page_SetLineWidth(page, lineWidth);
}

/**
 * Checks if given font and font size is already set in PDF gstate and if not,
 * update the gstate accordingly.
 * \param page		PDF page where the update needs to happen.
 * \param font		Wanted PDF font.
 * \param font_size	Wanted PDF font size.
 */
void pdfw_gs_font(HPDF_Page page, HPDF_Font font, HPDF_REAL font_size)
{
	if (font == pdfw_gs[pdfw_gs_level].font
		&& font_size == pdfw_gs[pdfw_gs_level].font_size)
		return;
	pdfw_gs[pdfw_gs_level].font = font;
	pdfw_gs[pdfw_gs_level].font_size = font_size;
	HPDF_Page_SetFontAndSize(page, font, font_size);
}

/**
 * Checks if given dash pattern is already set in PDF gstate and if not,
 * update the gstate accordingly.
 * \param page	PDF page where the update needs to happen.
 * \param dash	Wanted dash pattern.
 */
void pdfw_gs_dash(HPDF_Page page, DashPattern_e dash)
{
	if (dash == pdfw_gs[pdfw_gs_level].dash)
		return;
	pdfw_gs[pdfw_gs_level].dash = dash;
	switch (dash) {
		case DashPattern_eNone: {
			HPDF_Page_SetDash(page, NULL, 0, 0);
			break;
		}
		case DashPattern_eDash: {
			const HPDF_UINT16 dash_ptn[] = {3};
			HPDF_Page_SetDash(page, dash_ptn, 1, 1);
			break;
		}
		case DashPattern_eDotted: {
			const HPDF_UINT16 dash_ptn[] = {1};
			HPDF_Page_SetDash(page, dash_ptn, 1, 1);
			break;
		}
	}
}

#endif /* WITH_PDF_EXPORT */

