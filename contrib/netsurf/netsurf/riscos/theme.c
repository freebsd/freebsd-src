/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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
 * Window themes (implementation).
 */

#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "oslib/dragasprite.h"
#include "oslib/os.h"
#include "oslib/osgbpb.h"
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osspriteop.h"
#include "oslib/wimpspriteop.h"
#include "oslib/squash.h"
#include "oslib/wimp.h"
#include "oslib/wimpextend.h"
#include "oslib/wimpspriteop.h"
#include "content/content.h"
#include "desktop/gui.h"
#include "riscos/cookies.h"
#include "riscos/dialog.h"
#include "riscos/global_history.h"
#include "riscos/gui.h"
#include "riscos/hotlist.h"
#include "riscos/menus.h"
#include "utils/nsoption.h"
#include "riscos/theme.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/log.h"
#include "utils/utils.h"


/* \TODO -- provide a proper interface for these and make them static again!
 */

static struct theme_descriptor *theme_current = NULL;
static struct theme_descriptor *theme_descriptors = NULL;

static bool ro_gui_theme_add_descriptor(const char *folder, const char *leafname);
static void ro_gui_theme_get_available_in_dir(const char *directory);
static void ro_gui_theme_free(struct theme_descriptor *descriptor);

/**
 * Initialise the theme handler
 */
void ro_gui_theme_initialise(void)
{
	struct theme_descriptor *descriptor;

	theme_descriptors = ro_gui_theme_get_available();
	descriptor = ro_gui_theme_find(nsoption_charp(theme));
	if (!descriptor)
		descriptor = ro_gui_theme_find("Aletheia");
	ro_gui_theme_apply(descriptor);
}


/**
 * Finalise the theme handler
 */
void ro_gui_theme_finalise(void)
{
	ro_gui_theme_close(theme_current, false);
	ro_gui_theme_free(theme_descriptors);
}


/**
 * Finds a theme from the cached values.
 *
 * The returned theme is only guaranteed to be valid until the next call
 * to ro_gui_theme_get_available() unless it has been opened using
 * ro_gui_theme_open().
 *
 * \param leafname  the filename of the theme_descriptor to return
 * \return the requested theme_descriptor, or NULL if not found
 */
struct theme_descriptor *ro_gui_theme_find(const char *leafname)
{
	struct theme_descriptor *descriptor;

	if (!leafname)
		return NULL;

	for (descriptor = theme_descriptors; descriptor;
			descriptor = descriptor->next)
		if (!strcmp(leafname, descriptor->leafname))
			return descriptor;
	/* fallback for 10 chars on old filesystems */
	for (descriptor = theme_descriptors; descriptor;
			descriptor = descriptor->next)
		if (!strncmp(leafname, descriptor->leafname, 10))
			return descriptor;
	return NULL;
}


/**
 * Reads and caches the currently available themes.
 *
 * \return the requested theme_descriptor, or NULL if not found
 */
struct theme_descriptor *ro_gui_theme_get_available(void)
{
	struct theme_descriptor *current;
	struct theme_descriptor *test;

	/* close any unused descriptors */
	ro_gui_theme_free(theme_descriptors);

	/* add our default 'Aletheia' theme */
	ro_gui_theme_add_descriptor("NetSurf:Resources", "Aletheia");

	/* scan our choices directory */
	ro_gui_theme_get_available_in_dir(nsoption_charp(theme_path));

	/* sort alphabetically in a very rubbish way */
	if ((theme_descriptors) && (theme_descriptors->next)) {
		current = theme_descriptors;
		while ((test = current->next)) {
			if (strcmp(current->name, test->name) > 0) {
				current->next->previous = current->previous;
				if (current->previous)
					current->previous->next = current->next;
				current->next = test->next;
				test->next = current;
				current->previous = test;
				if (current->next)
					current->next->previous = current;

				current = test->previous;
				if (!current) current = test;
			} else {
				current = current->next;
			}
		}
		while (theme_descriptors->previous)
			theme_descriptors = theme_descriptors->previous;
	}

	return theme_descriptors;
}


/**
 * Adds the themes in a directory to the global cache.
 *
 * \param directory  the directory to scan
 */
static void ro_gui_theme_get_available_in_dir(const char *directory)
{
	int context = 0;
	int read_count;
	osgbpb_INFO(100) info;
	os_error *error;

	while (context != -1) {
	  	/* read some directory info */
		error = xosgbpb_dir_entries_info(directory,
				(osgbpb_info_list *) &info, 1, context,
				sizeof(info), 0, &read_count, &context);
		if (error) {
			LOG(("xosgbpb_dir_entries_info: 0x%x: %s",
				error->errnum, error->errmess));
			if (error->errnum == 0xd6)	/* no such dir */
				return;
			warn_user("MiscError", error->errmess);
			break;
		}

		/* only process files */
		if ((read_count != 0) && (info.obj_type == fileswitch_IS_FILE))
			ro_gui_theme_add_descriptor(directory, info.name);
	}
}


/**
 * Returns the current theme handle, or NULL if none is set.
 *
 * \return		The theme descriptor handle, or NULL.
 */

struct theme_descriptor *ro_gui_theme_get_current(void)
{
	return theme_current;
}


/**
 * Returns a sprite area for use with the given theme.  This may return a
 * pointer to the wimp sprite pool if a theme area isn't available.
 *
 * \param *descriptor		The theme to use, or NULL for the current.
 * \return			A pointer to the theme sprite area.
 */

osspriteop_area *ro_gui_theme_get_sprites(struct theme_descriptor *descriptor)
{
	osspriteop_area		*area;

	if (descriptor == NULL)
		descriptor = theme_current;

	if (descriptor != NULL && descriptor->theme != NULL)
		area = descriptor->theme->sprite_area;
	else
		area = (osspriteop_area *) 1;

	return area;
}


/**
 * Returns an interger element from the specified theme, or the current theme
 * if the descriptor is NULL.
 *
 * This is an attempt to abstract the theme data from its clients: it should
 * simplify the task of expanding the theme system in the future should this
 * be necessary to include other parts of the RISC OS GUI in the theme system.
 *
 * \param *descriptor		The theme to use, or NULL for the current.
 * \param style			The style to use.
 * \param element		The style element to return.
 * \return			The requested value, or 0.
 */

int ro_gui_theme_get_style_element(struct theme_descriptor *descriptor,
		theme_style style, theme_element element)
{
	if (descriptor == NULL)
		descriptor = theme_current;

	if (descriptor == NULL)
		return 0;

	switch (style) {
	case THEME_STYLE_NONE:
		switch(element) {
		case THEME_ELEMENT_FOREGROUND:
			return wimp_COLOUR_BLACK;
		case THEME_ELEMENT_BACKGROUND:
			return wimp_COLOUR_VERY_LIGHT_GREY;
		default:
			return 0;
		}
		break;

	case THEME_STYLE_BROWSER_TOOLBAR:
		switch (element) {
		case THEME_ELEMENT_FOREGROUND:
			return wimp_COLOUR_BLACK;
		case THEME_ELEMENT_BACKGROUND:
			return descriptor->browser_background;
		default:
			return 0;
		}
		break;

	case THEME_STYLE_HOTLIST_TOOLBAR:
	case THEME_STYLE_COOKIES_TOOLBAR:
	case THEME_STYLE_GLOBAL_HISTORY_TOOLBAR:
		switch (element) {
		case THEME_ELEMENT_FOREGROUND:
			return wimp_COLOUR_BLACK;
		case THEME_ELEMENT_BACKGROUND:
			return descriptor->hotlist_background;
		default:
			return 0;
		}
		break;

	case THEME_STYLE_STATUS_BAR:
		switch (element) {
		case THEME_ELEMENT_FOREGROUND:
			return descriptor->status_foreground;
		case THEME_ELEMENT_BACKGROUND:
			return descriptor->status_background;
		default:
			return 0;
		}
		break;

	default:
		return 0;
	}
}

/**
 * Returns details of the throbber as defined in a theme.
 *
 * \param *descriptor		The theme of interest (NULL for current).
 * \param *frames		Return the number of animation frames.
 * \param *width		Return the throbber width.
 * \param *height		Return the throbber height.
 * \param *right		Return the 'locate on right' flag.
 * \param *redraw		Return the 'forcible redraw' flag.
 * \return			true if meaningful data has been returned;
 *				else false.
 */

bool ro_gui_theme_get_throbber_data(struct theme_descriptor *descriptor,
		int *frames, int *width, int *height,
		bool *right, bool *redraw)
{
	if (descriptor == NULL)
		descriptor = theme_current;

	if (descriptor == NULL || descriptor->theme == NULL)
		return false;

	if (frames != NULL)
		*frames = descriptor->theme->throbber_frames;
	if (width != NULL)
		*width = descriptor->theme->throbber_width;
	if (height != NULL)
		*height = descriptor->theme->throbber_height;
	if (right != NULL)
		*right = descriptor->throbber_right;
	if (redraw != NULL)
		*redraw = descriptor->throbber_redraw;

	return true;
}


/**
 * Checks a theme is valid and adds it to the current list
 *
 * \param folder	the theme folder
 * \param leafname	the theme leafname
 * \return whether the theme was added
 */
bool ro_gui_theme_add_descriptor(const char *folder, const char *leafname)
{
	struct theme_file_header file_header;
	struct theme_descriptor *current;
	struct theme_descriptor *test;
	int output_left;
	os_fw file_handle;
	os_error *error;
	char *filename;

	/* create a full filename */
	filename = malloc(strlen(folder) + strlen(leafname) + 2);
	if (!filename) {
	  	LOG(("No memory for malloc"));
	  	warn_user("NoMemory", 0);
	  	return false;
	}
	sprintf(filename, "%s.%s", folder, leafname);

	/* get the header */
	error = xosfind_openinw(osfind_NO_PATH, filename, 0,
			&file_handle);
	if (error) {
		LOG(("xosfind_openinw: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("FileError", error->errmess);
		free(filename);
		return false;
	}
	if (file_handle == 0) {
		free(filename);
		return false;
	}
	error = xosgbpb_read_atw(file_handle,
			(byte *) &file_header,
			sizeof (struct theme_file_header),
			0, &output_left);
	xosfind_closew(file_handle);
	if (error) {
		LOG(("xosbgpb_read_atw: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("FileError", error->errmess);
		free(filename);
		return false;
	}
	if (output_left > 0) {	/* should try to read more? */
	  	free(filename);
	  	return false;
	}

	/* create a new theme descriptor */
	current = (struct theme_descriptor *)calloc(1,
			sizeof(struct theme_descriptor));
	if (!current) {
		LOG(("calloc failed"));
		warn_user("NoMemory", 0);
		free(filename);
		return false;
	}
	if (!ro_gui_theme_read_file_header(current, &file_header)) {
		free(filename);
		free(current);
		return false;
	}
	current->filename = filename;
	current->leafname = current->filename + strlen(folder) + 1;

	/* don't add duplicates */
	for (test = theme_descriptors; test; test = test->next) {
		if (!strcmp(current->name, test->name)) {
			free(current->filename);
			free(current);
			return false;
		}
	}

	/* link in our new descriptor at the head*/
	if (theme_descriptors) {
		current->next = theme_descriptors;
		theme_descriptors->previous = current;
	}
	theme_descriptors = current;
	return true;

}


/**
 * Fills in the basic details for a descriptor from a file header.
 * The filename string is not set.
 *
 * \param descriptor   the descriptor to set up
 * \param file_header  the header to read from
 * \return false for a badly formed theme, true otherwise
 */
bool ro_gui_theme_read_file_header(struct theme_descriptor *descriptor,
		struct theme_file_header *file_header)
{
	if ((file_header->magic_value != 0x4d54534e) ||
			(file_header->parser_version > 2))
		return false;

	strcpy(descriptor->name, file_header->name);
	strcpy(descriptor->author, file_header->author);
	descriptor->browser_background = file_header->browser_bg;
	descriptor->hotlist_background = file_header->hotlist_bg;
	descriptor->status_background = file_header->status_bg;
	descriptor->status_foreground = file_header->status_fg;
	descriptor->decompressed_size = file_header->decompressed_sprite_size;
	descriptor->compressed_size = file_header->compressed_sprite_size;
	if (file_header->parser_version >= 2) {
		descriptor->throbber_right =
				!(file_header->theme_flags & (1 << 0));
		descriptor->throbber_redraw =
				file_header->theme_flags & (1 << 1);
	} else {
		descriptor->throbber_right =
				(file_header->theme_flags == 0x00);
		descriptor->throbber_redraw = true;
	}
	return true;
}


/**
 * Opens a theme ready for use.
 *
 * \param descriptor  the theme_descriptor to open
 * \param list	      whether to open all themes in the list
 * \return whether the operation was successful
 */
bool ro_gui_theme_open(struct theme_descriptor *descriptor, bool list)
{
	fileswitch_object_type obj_type;
	squash_output_status status;
	os_coord dimensions;
	os_mode mode;
	os_error *error;
	struct theme_descriptor *next_descriptor;
	char sprite_name[16];
	const char *name = sprite_name;
	bool result = true;
	int i, n;
	int workspace_size, file_size;
	char *raw_data, *workspace;
	osspriteop_area *decompressed;

	/*	If we are freeing the whole of the list then we need to
		start at the first descriptor.
	*/
	if (list && descriptor)
		while (descriptor->previous) descriptor = descriptor->previous;

	/*	Open the themes
	*/
	for (; descriptor; descriptor = next_descriptor) {
		/* see if we should iterate through the entire list */
		if (list)
			next_descriptor = descriptor->next;
		else
			next_descriptor = NULL;

		/* if we are already loaded, increase the usage count */
		if (descriptor->theme) {
			descriptor->theme->users = descriptor->theme->users + 1;
			continue;
		}

		/* create a new theme */
		descriptor->theme = (struct theme *)calloc(1,
				sizeof(struct theme));
		if (!descriptor->theme) {
			LOG(("calloc() failed"));
			warn_user("NoMemory", 0);
			continue;
		}
		descriptor->theme->users = 1;

		/* try to load the associated file */
		error = xosfile_read_stamped_no_path(descriptor->filename,
				&obj_type, 0, 0, &file_size, 0, 0);
		if (error) {
			LOG(("xosfile_read_stamped_no_path: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("FileError", error->errmess);
			continue;
		}
		if (obj_type != fileswitch_IS_FILE)
			continue;
		raw_data = malloc(file_size);
		if (!raw_data) {
			LOG(("malloc() failed"));
			warn_user("NoMemory", 0);
			continue;
		}
		error = xosfile_load_stamped_no_path(descriptor->filename,
				(byte *)raw_data, 0, 0, 0, 0, 0);
		if (error) {
			free(raw_data);
			LOG(("xosfile_load_stamped_no_path: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("FileError", error->errmess);
			continue;
		}

		/* decompress the new data */
		error = xsquash_decompress_return_sizes(-1, &workspace_size, 0);
		if (error) {
			free(raw_data);
			LOG(("xsquash_decompress_return_sizes: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			continue;
		}
		decompressed = (osspriteop_area *)malloc(
				descriptor->decompressed_size);
		workspace = malloc(workspace_size);
		if ((!decompressed) || (!workspace)) {
			free(decompressed);
			free(raw_data);
			LOG(("malloc() failed"));
			warn_user("NoMemory", 0);
			continue;
		}
		error = xsquash_decompress(squash_INPUT_ALL_PRESENT, workspace,
				(byte *)(raw_data + sizeof(
						struct theme_file_header)),
				descriptor->compressed_size,
				(byte *)decompressed,
				descriptor->decompressed_size,
				&status, 0, 0, 0, 0);
		free(workspace);
		free(raw_data);
		if (error) {
			free(decompressed);
			LOG(("xsquash_decompress: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
			continue;
		}
		if (status != 0) {
			free(decompressed);
			continue;
		}
		descriptor->theme->sprite_area = decompressed;

		/* find the highest sprite called 'throbber%i', and get the
		 * maximum dimensions for all 'thobber%i' icons. */
		for (i = 1; i <= descriptor->theme->sprite_area->sprite_count;
				i++) {
			error = xosspriteop_return_name(osspriteop_USER_AREA,
					descriptor->theme->sprite_area,
					sprite_name, 16, i, 0);
			if (error) {
				LOG(("xosspriteop_return_name: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
				continue;
			}
			if (strncmp(sprite_name, "throbber", 8))
				continue;

			/* get the max sprite width/height */
			error = xosspriteop_read_sprite_info(
					osspriteop_USER_AREA,
					descriptor->theme->sprite_area,
					(osspriteop_id) name,
					&dimensions.x, &dimensions.y,
					(osbool *) 0, &mode);
			if (error) {
				LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
				continue;
			}
			ro_convert_pixels_to_os_units(&dimensions, mode);
			if (descriptor->theme->throbber_width <	dimensions.x)
				descriptor->theme->throbber_width =
						dimensions.x;
			if (descriptor->theme->throbber_height < dimensions.y)
				descriptor->theme->throbber_height =
						dimensions.y;

			/* get the throbber number */
			n = atoi(sprite_name + 8);
			if (descriptor->theme->throbber_frames < n)
				descriptor->theme->throbber_frames = n;
		}
	}
	return result;
}


/**
 * Applies the theme to all current windows and subsequent ones.
 *
 * \param descriptor  the theme_descriptor to open
 * \return whether the operation was successful
 */
bool ro_gui_theme_apply(struct theme_descriptor *descriptor)
{
	struct theme_descriptor *theme_previous;

	/* check if the theme is already applied */
	if (descriptor == theme_current)
		return true;

	/* re-open the new-theme and release the current theme */
	if (!ro_gui_theme_open(descriptor, false))
		return false;
	theme_previous = theme_current;
	theme_current = descriptor;

	/* apply the theme to all the current toolbar-ed windows */
	ro_toolbar_theme_update();

	ro_gui_theme_close(theme_previous, false);
	return true;
}


/**
 * Closes a theme after use.
 *
 * \param descriptor  the theme_descriptor to close
 * \param list	      whether to open all themes in the list
 * \return whether the operation was successful
 */
void ro_gui_theme_close(struct theme_descriptor *descriptor, bool list)
{

	if (!descriptor)
		return;

	/* move to the start of the list */
	while (list && descriptor->previous)
		descriptor = descriptor->previous;

	/* close the themes */
	while (descriptor) {
		if (descriptor->theme) {
			descriptor->theme->users = descriptor->theme->users - 1;
			if (descriptor->theme->users <= 0) {
				free(descriptor->theme->sprite_area);
				free(descriptor->theme);
				descriptor->theme = NULL;
			}
		}
		if (!list)
			return;
		descriptor = descriptor->next;
	}
}


/**
 * Frees any unused theme descriptors.
 *
 * \param descriptor  the theme_descriptor to free
 * \param list	      whether to open all themes in the list
 * \return whether the operation was successful
 */
void ro_gui_theme_free(struct theme_descriptor *descriptor)
{
	struct theme_descriptor *next_descriptor;

	if (!descriptor)
		return;

	/* move to the start of the list */
	while (descriptor->previous)
		descriptor = descriptor->previous;

	/* free closed themes */
	for (; descriptor; descriptor = next_descriptor) {
		next_descriptor = descriptor->next;

		/* no theme? no descriptor */
		if (!descriptor->theme) {
			if (descriptor->previous)
				descriptor->previous->next = descriptor->next;
			if (descriptor->next)
				descriptor->next->previous =
						descriptor->previous;

			/* keep the cached list in sync */
			if (theme_descriptors == descriptor)
				theme_descriptors = next_descriptor;

			/* release any memory */
			free(descriptor->filename);
			free(descriptor);
		}
	}
}


