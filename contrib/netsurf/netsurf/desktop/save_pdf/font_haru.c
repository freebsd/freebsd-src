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
  * Font handling in Haru pdf documents (implementation).
  *
  * The functions were written to implement the same interface as the Pango ones
  * so that the usage of the latter wouldn't have to be modified.
  */

#include "utils/config.h"
#ifdef WITH_PDF_EXPORT

/*#define FONT_HARU_DEBUG */
 
#include <assert.h> 
#include <float.h> 
#include <math.h>
#include <string.h> 

#include <hpdf.h>

#include "css/css.h"
#include "css/utils.h"

#include "utils/nsoption.h"
#include "desktop/save_pdf/font_haru.h"
#include "render/font.h"
#include "utils/log.h"


static bool haru_nsfont_init(HPDF_Doc *pdf, HPDF_Page *page,
		const char *string, char **string_nt, int length);

static bool haru_nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width);

static bool haru_nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);

static bool haru_nsfont_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
	 	int x, size_t *char_offset, int *actual_x);
	 	
static float pdf_text_scale = DEFAULT_EXPORT_SCALE;

const struct font_functions haru_nsfont = {
	haru_nsfont_width,
 	haru_nsfont_position_in_string,
 	haru_nsfont_split
};

/**
 * Haru error handler
 * for debugging purposes - it immediately exits the program on the first error,
 * as it would otherwise flood the user with all resulting complications,
 * covering the most important error source.
 */
static void error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no,
		void *user_data)
{
	LOG(("ERROR: in font_haru \n\terror_no=%x\n\tdetail_no=%d\n", 
			(HPDF_UINT)error_no, (HPDF_UINT)detail_no));
#ifdef FONT_HARU_DEBUG	
	exit(1);
#endif	
}

static bool haru_nsfont_init(HPDF_Doc *pdf, HPDF_Page *page,
		const char *string, char **string_nt, int length)
{
	
	*pdf = HPDF_New(error_handler, NULL);
	
	if (*pdf == NULL)
		return false;

	*page = HPDF_AddPage(*pdf);
	
	if (*page == NULL) {
		HPDF_Free(*pdf);
		return false;	
	}
	
	*string_nt = malloc((length + 1) * sizeof(char));
	if (*string_nt == NULL) {
		HPDF_Free(*pdf);
		return false;
	}
	
	memcpy(*string_nt, string, length);
	(*string_nt)[length] = '\0';	
	return true;
}

/**
 * Measure the width of a string.
 *
 * \param  fstyle  style for this text
 * \param  string  string to measure (no UTF-8 currently)
 * \param  length  length of string
 * \param  width   updated to width of string[0..length]
 * \return  true on success, false on error and error reported
 */
bool haru_nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
	 	int *width)
{
	HPDF_Doc pdf;
	HPDF_Page page;
	char *string_nt;
	HPDF_REAL width_real;

	*width = 0;

	if (length == 0)
		return true;

	if (!haru_nsfont_init(&pdf, &page, string, &string_nt, length))
		return false;

	if (!haru_nsfont_apply_style(fstyle, pdf, page, NULL, NULL)) {
		free(string_nt);
		HPDF_Free(pdf);
		return false;
	}

	width_real = HPDF_Page_TextWidth(page, string_nt);
	*width = width_real;

#ifdef FONT_HARU_DEBUG		
	LOG(("Measuring string: %s ; Calculated width: %f %i",string_nt, width_real, *width));
#endif
	free(string_nt);
	HPDF_Free(pdf);

	return true;
}


/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param  fstyle	style for this text
 * \param  string	string to measure (no UTF-8 currently)
 * \param  length	length of string
 * \param  x		x coordinate to search for
 * \param  char_offset	updated to offset in string of actual_x, [0..length]
 * \param  actual_x	updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

bool haru_nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	HPDF_Doc pdf;
	HPDF_Page page;
	char *string_nt;
	HPDF_UINT offset;
	HPDF_REAL real_width;
	
	if (!haru_nsfont_init(&pdf, &page, string, &string_nt, length))
		return false;
	
	if (HPDF_Page_SetWidth(page, x) != HPDF_OK
			|| !haru_nsfont_apply_style(fstyle, pdf, page, NULL, NULL)) {
		free(string_nt);
		HPDF_Free(pdf);
		return false;
	}

	
	offset = HPDF_Page_MeasureText(page, string_nt, x,
			HPDF_FALSE, &real_width);
	

	if (real_width < x)
		*char_offset = offset;
	else {
		assert(fabs(real_width - x) < FLT_EPSILON);
		assert(offset > 0);
		*char_offset = offset - 1;
	}
	
	/*TODO: this is only the right edge of the character*/
	*actual_x = real_width;
	
#ifdef FONT_HARU_DEBUG	
	LOG(("Position in string: %s at x: %i; Calculated position: %i",
			string_nt, x, *char_offset));	
#endif	
	free(string_nt);
	HPDF_Free(pdf);
	
	return true;
}

/**
 * Find where to split a string to make it fit a width.
 *
 * \param  fstyle	style for this text
 * \param  string	string to measure (no UTF-8 currently)
 * \param  length	length of string
 * \param  x		width available
 * \param  char_offset	updated to offset in string of actual_x, [0..length]
 * \param  actual_x	updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

bool haru_nsfont_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	HPDF_Doc pdf;
	HPDF_Page page;
	char *string_nt;
	HPDF_REAL real_width;
	HPDF_UINT offset;
	
	
	if (!haru_nsfont_init(&pdf, &page, string, &string_nt, length))
		return false;
	
	if (HPDF_Page_SetWidth(page, x) != HPDF_OK
		    || !haru_nsfont_apply_style(fstyle, pdf, page, NULL, NULL)) {
		free(string_nt);
		HPDF_Free(pdf);
		return false;
	}
	
	offset = HPDF_Page_MeasureText(page, string_nt, x,
			HPDF_TRUE, &real_width);
	
#ifdef FONT_HARU_DEBUG	
	LOG(("Splitting string: %s for width: %i ; Calculated position: %i Calculated real_width: %f", 
	string_nt, x, *char_offset, real_width));	
#endif	
	*char_offset = offset - 1;
	
	/*TODO: this is only the right edge of the character*/
	*actual_x = real_width;
	
	free(string_nt);
	HPDF_Free(pdf);
	
	return true;	
}

/**
 * Apply font style to a Haru HPDF_Page
 *
 * \param  fstyle	plot style for this page
 * \param  doc		document owning the page
 * \param  page		the page to apply the style to
 * \param  font		if this is non NULL it is updated to the font based
 *			on given style
 * \param  font_size	if this is non NULL it is updated to the font size
 *			based on given style
 * \return true on success, false on error and error reported
 *
 * When both font and font_size are NULL, the HPDF_Page is updated for given
 * style, otherwise it is left to the called to do this.
 */
bool haru_nsfont_apply_style(const plot_font_style_t *fstyle,
		HPDF_Doc doc, HPDF_Page page,
		HPDF_Font *font, HPDF_REAL *font_size)
{
	HPDF_Font pdf_font;
	HPDF_REAL size;
	char font_name[50];
	bool roman = false;
	bool bold = false;
	bool styled = false;

	/*TODO: style handling, we are mapping the
		styles on the basic 14 fonts only
	*/
	switch (fstyle->family) {
	case PLOT_FONT_FAMILY_SERIF:
		strcpy(font_name, "Times");
		roman = true;
		break;
	case PLOT_FONT_FAMILY_MONOSPACE:
		strcpy(font_name, "Courier");
		break;
	case PLOT_FONT_FAMILY_SANS_SERIF:
		strcpy(font_name, "Helvetica");
		break;
	case PLOT_FONT_FAMILY_CURSIVE:			
	case PLOT_FONT_FAMILY_FANTASY:		
	default:
		strcpy(font_name, "Times");
		roman=true;
		break;
	}
	
	if (fstyle->weight == 700) {
		strcat(font_name, "-Bold");
		bold = true;
	}

	if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE)) {
		if (!bold)
			strcat(font_name,"-");
		if (roman)
			strcat(font_name,"Italic");
		else
			strcat(font_name,"Oblique");
			
		styled = true;
	}
	
	if (roman && !styled && !bold)
		strcat(font_name, "-Roman");

#ifdef FONT_HARU_DEBUG		
	LOG(("Setting font: %s", font_name));
#endif		

	size = fstyle->size;

	if (font != NULL)
		size *= pdf_text_scale;

	if (size <= 0)
		return true;

	size /= FONT_SIZE_SCALE;

	if (size > HPDF_MAX_FONTSIZE)
		size = HPDF_MAX_FONTSIZE;

	if (font_size)
		*font_size = size;

	pdf_font = HPDF_GetFont(doc, font_name, "StandardEncoding");
	if (pdf_font == NULL)
		return false;
	if (font != NULL)
		*font = pdf_font;

	if (font == NULL || font_size == NULL)
		HPDF_Page_SetFontAndSize(page, pdf_font, size);
	
	return true;
}

/**
 * Sync the text scale with the scale for the whole content
 */
void haru_nsfont_set_scale(float s)
{
	pdf_text_scale = s;
}

#endif /* WITH_PDF_EXPORT */

