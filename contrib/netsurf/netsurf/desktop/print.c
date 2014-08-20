/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
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
 * Output-in-pages implementation
*/

#include "utils/config.h"

#include <assert.h>
#include <string.h>

#include <dom/dom.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "css/utils.h"
#include "utils/nsoption.h"
#include "desktop/print.h"
#include "desktop/printer.h"
#include "render/box.h"
#include "utils/log.h"
#include "utils/talloc.h"
#include "utils/types.h"

/* Default print settings */
#define DEFAULT_PAGE_WIDTH 595
#define DEFAULT_PAGE_HEIGHT 840
#define DEFAULT_COPIES 1

static hlcache_handle *print_init(hlcache_handle *, struct print_settings *);
static bool print_apply_settings(hlcache_handle *, struct print_settings *);

static float page_content_width, page_content_height;
static hlcache_handle *printed_content;
static float done_height;

bool html_redraw_printing = false;
int html_redraw_printing_border = 0;
int html_redraw_printing_top_cropped = 0;

/**
 * This function calls print setup, prints page after page until the whole
 * content is printed calls cleaning up afterwise.
 *
 * \param content The content to be printed
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
bool print_basic_run(hlcache_handle *content,
		const struct printer *printer,
		struct print_settings *settings)
{
	bool ret = true;

	assert(content != NULL && printer != NULL && settings != NULL);
	
	if (print_set_up(content, printer, settings, NULL))
		return false;

	while (ret && (done_height < content_get_height(printed_content)) ) {
		ret = print_draw_next_page(printer, settings);
	}

	print_cleanup(content, printer, settings);
	
	return ret;
}

/**
 * This function prepares the content to be printed. The current browser content
 * is duplicated and resized, printer initialization is called.
 *
 * \param content The content to be printed
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use
 * \param height updated to the height of the printed content
 * \return true if successful, false otherwise
 */
bool print_set_up(hlcache_handle *content,
		const struct printer *printer, struct print_settings *settings,
		double *height)
{
	printed_content = print_init(content, settings);
	
	if (printed_content == NULL)
		return false;
	
	print_apply_settings(printed_content, settings);

	if (height)
		*height = content_get_height(printed_content);
	
	printer->print_begin(settings);

	done_height = 0;
	
	return true;	
}

/**
 * This function draws one page, beginning with the height offset of done_height
 *
 * \param printer The printer interface for the printer to be used
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
bool print_draw_next_page(const struct printer *printer,
		struct print_settings *settings)
{
	struct rect clip;
	struct content_redraw_data data;
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = !nsoption_bool(remove_backgrounds),
		.plot = printer->plotter
	};

	html_redraw_printing_top_cropped = INT_MAX;

	clip.x0 = 0;
	clip.y0 = 0;
	clip.x1 = page_content_width * settings->scale;
	clip.y1 = page_content_height  * settings->scale;

	data.x = 0;
	data.y = -done_height;
	data.width = 0;
	data.height = 0;
	data.background_colour = 0xFFFFFF;
	data.scale = settings->scale;
	data.repeat_x = false;
	data.repeat_y = false;

	html_redraw_printing = true;
	html_redraw_printing_border = clip.y1;

	printer->print_next_page();
	if (!content_redraw(printed_content, &data, &clip, &ctx))
		return false;

	done_height += page_content_height -
			(html_redraw_printing_top_cropped != INT_MAX ?
			clip.y1 - html_redraw_printing_top_cropped : 0) / 
			settings->scale;

	return true;
}

/**
 * The content passed to the function is duplicated with its boxes, font
 * measuring functions are being set.
 *
 * \param content The content to be printed
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
hlcache_handle *print_init(hlcache_handle *content,
		struct print_settings *settings)
{
	hlcache_handle* printed_content;
	
	hlcache_handle_clone(content, &printed_content);
			
	return printed_content;
}

/**
 * The content is resized to fit page width.
 *
 * \param content The content to be printed
 * \param settings The settings for printing to use
 * \return true if successful, false otherwise
 */
bool print_apply_settings(hlcache_handle *content,
			  struct print_settings *settings)
{
	if (settings == NULL)
		return false;
	
	/*Apply settings - adjust page size etc*/

	page_content_width = (settings->page_width - 
			FIXTOFLT(FSUB(settings->margins[MARGINLEFT],
			settings->margins[MARGINRIGHT]))) / settings->scale;
	
	page_content_height = (settings->page_height - 
			FIXTOFLT(FSUB(settings->margins[MARGINTOP],
			settings->margins[MARGINBOTTOM]))) / settings->scale;
	
	content_reformat(content, false, page_content_width, 0);

	LOG(("New layout applied.New height = %d ; New width = %d ",
			content_get_height(content), 
			content_get_width(content)));
	
	return true;
}

/**
 * Memory allocated during printing is being freed here.
 *
 * \param content The original content
 * \param printer The printer interface for the printer to be used
 * \return true if successful, false otherwise
 */
bool print_cleanup(hlcache_handle *content, const struct printer *printer,
		struct print_settings *settings)
{
	printer->print_end();
	
	html_redraw_printing = false;
	
	if (printed_content) {
		hlcache_handle_release(printed_content);
	}
	
	free((void *)settings->output);
	free(settings);
	
	return true;
}

/**
 * Generates one of the predefined print settings sets.
 *
 * \param configuration the requested configuration
 * \param filename the filename or NULL
 * \param font handling functions
 * \return print_settings in case if successful, NULL if unknown configuration \
 * 	or lack of memory.
 */
struct print_settings *print_make_settings(print_configuration configuration,
		const char *filename, const struct font_functions *font_func)
{
	struct print_settings *settings;
	css_fixed length = 0;
	css_unit unit = CSS_UNIT_MM;
	
	switch (configuration){
		case PRINT_DEFAULT:	
			settings = (struct print_settings*) 
					malloc(sizeof(struct print_settings));
			if (settings == NULL)
				return NULL;
			
			settings->page_width  = DEFAULT_PAGE_WIDTH;
			settings->page_height = DEFAULT_PAGE_HEIGHT;
			settings->copies = DEFAULT_COPIES;

			settings->scale = DEFAULT_EXPORT_SCALE;
			
			length = INTTOFIX(DEFAULT_MARGIN_LEFT_MM);
			settings->margins[MARGINLEFT] = 
					nscss_len2px(length, unit, NULL);
			length = INTTOFIX(DEFAULT_MARGIN_RIGHT_MM);
			settings->margins[MARGINRIGHT] = 
					nscss_len2px(length, unit, NULL);
			length = INTTOFIX(DEFAULT_MARGIN_TOP_MM);
			settings->margins[MARGINTOP] = 
					nscss_len2px(length, unit, NULL);
			length = INTTOFIX(DEFAULT_MARGIN_BOTTOM_MM);
			settings->margins[MARGINBOTTOM] = 
					nscss_len2px(length, unit, NULL);
			break;
		/* use settings from the Export options tab */
		case PRINT_OPTIONS:
			settings = (struct print_settings*) 
					malloc(sizeof(struct print_settings));
			if (settings == NULL)
				return NULL;
			
			settings->page_width  = DEFAULT_PAGE_WIDTH;
			settings->page_height = DEFAULT_PAGE_HEIGHT;
			settings->copies = DEFAULT_COPIES;
			
			settings->scale = (float)nsoption_int(export_scale) / 100;
			
			length = INTTOFIX(nsoption_int(margin_left));
			settings->margins[MARGINLEFT] = 
					nscss_len2px(length, unit, NULL);
			length = INTTOFIX(nsoption_int(margin_right));
			settings->margins[MARGINRIGHT] = 
					nscss_len2px(length, unit, NULL);
			length = INTTOFIX(nsoption_int(margin_top));
			settings->margins[MARGINTOP] = 
					nscss_len2px(length, unit, NULL);
			length = INTTOFIX(nsoption_int(margin_bottom));
			settings->margins[MARGINBOTTOM] = 
					nscss_len2px(length, unit, NULL);
			break;
		default:
			return NULL;
	}

	/* Set font functions */
	settings->font_func = font_func;

	/* Output filename, or NULL if printing */
	if (filename != NULL) {
		settings->output = strdup(filename);
		if (settings->output == NULL) {
			free(settings);
			return NULL;
		}
	} else 
		settings->output = NULL;

	return settings;	
}

