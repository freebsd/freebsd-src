/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 * Copyright 2010, 2011 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Window toolbars (implementation).
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
#include "desktop/plotters.h"
#include "riscos/cookies.h"
#include "riscos/dialog.h"
#include "riscos/global_history.h"
#include "riscos/gui.h"
#include "riscos/gui/button_bar.h"
#include "riscos/gui/throbber.h"
#include "riscos/gui/url_bar.h"
#include "riscos/hotlist.h"
#include "riscos/menus.h"
#include "utils/nsoption.h"
#include "riscos/save.h"
#include "riscos/theme.h"
#include "riscos/toolbar.h"
#include "riscos/treeview.h"
#include "riscos/url_complete.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "riscos/window.h"
#include "utils/log.h"
#include "utils/utils.h"


#define TOOLBAR_WIDGET_GUTTER 8
#define TOOLBAR_DEFAULT_WIDTH 16384

/* Toolbar rows used to index into the arrays of row-specific data.
 */

#define TOOLBAR_ROW_TOP   0
#define TOOLBAR_ROW_DIV1  1
#define TOOLBAR_ROW_EDIT  2
#define TOOLBAR_MAX_ROWS  3

/* The toolbar data structure.
 */

struct toolbar {
	/** Bar details. */
	struct theme_descriptor		*theme;
	theme_style			style;
	toolbar_flags			flags;

	int				current_width, current_height;
	int				full_width, full_height;
	int				clip_width, clip_height;

	/** Toolbar and parent window handles. */
	wimp_w				toolbar_handle;
	wimp_w				parent_handle;

	/** Row locations and sizes. */
	int				row_y0[TOOLBAR_MAX_ROWS];
	int				row_y1[TOOLBAR_MAX_ROWS];

	/** Details for the button bar. */
	struct button_bar		*buttons;
	bool				buttons_display;
	os_coord			buttons_size;

	/** Details for the URL bar. */
	struct url_bar			*url;
	bool				url_display;
	os_coord			url_size;

	/** Details for the throbber. */
	struct throbber			*throbber;
	bool				throbber_display;
	bool				throbber_right;
	os_coord			throbber_size;

	/** Client callback data. */
	const struct toolbar_callbacks	*callbacks;
	void				*client_data;

	/** Details for the toolbar editor. */
	wimp_i				editor_div1;
	struct button_bar		*editor;
	os_coord			editor_size;

	bool				editing;

	/** Interactive help data. */

	const char			*help_prefix;

	/** The next bar in the toolbar list. */
	struct toolbar			*next;
};


/* Global variables for the toolbar module.
 */

/** The list of defined toolbars. */
static struct toolbar			*ro_toolbar_bars = NULL;

/** The Toolber Menu */
wimp_menu				*toolbar_menu;


/*	A basic window definition for the toolbar and status bar.
 */

static wimp_window ro_toolbar_window = {
	{0, 0, 1, 1},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE | wimp_WINDOW_NO_BOUNDS |
			wimp_WINDOW_FURNITURE_WINDOW |
			wimp_WINDOW_IGNORE_XEXTENT | wimp_WINDOW_IGNORE_YEXTENT,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_VERY_LIGHT_GREY,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	wimp_WINDOW_NEVER3D | 0x16u /* RISC OS 5.03+ */,
	{0, 0, TOOLBAR_DEFAULT_WIDTH, 16384},
	0,
	wimp_BUTTON_DOUBLE_CLICK_DRAG << wimp_ICON_BUTTON_TYPE_SHIFT,
	wimpspriteop_AREA,
	1,
	1,
	{""},
	0,
	{ }
};

static char ro_toolbar_null_string[] = "";
static char ro_toolbar_line_validation[] = "R2";

/*
 * Private function prototypes.
 */

static void ro_toolbar_update_current_widgets(struct toolbar *toolbar);
static void ro_toolbar_refresh_widget_dimensions(struct toolbar *toolbar);
static void ro_toolbar_reformat_widgets(struct toolbar *toolbar);

static void ro_toolbar_redraw(wimp_draw *redraw);
static bool ro_toolbar_click(wimp_pointer *pointer);
static bool ro_toolbar_keypress(wimp_key *key);
static bool ro_toolbar_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer);
static void ro_toolbar_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static bool ro_toolbar_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static const char *ro_toolbar_get_help_suffix(wimp_w w, wimp_i i, os_coord *pos,
		wimp_mouse_state buttons);

static void ro_toolbar_update_buttons(struct toolbar *toolbar);


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_init(void)
{
	/* browser toolbar menu */
	static const struct ns_menu toolbar_definition = {
		"Toolbar", {
			{ "Toolbars", NO_ACTION, 0 },
			{ "Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Toolbars.ToolAddress", TOOLBAR_ADDRESS_BAR, 0 },
			{ "Toolbars.ToolThrob", TOOLBAR_THROBBER, 0 },
			{ "EditToolbar", TOOLBAR_EDIT, 0 },
			{NULL, 0, 0}
		}
	};
	toolbar_menu = ro_gui_menu_define_menu(
			&toolbar_definition);
}


/* This is an exported interface documented in toolbar.h */

struct toolbar *ro_toolbar_create(struct theme_descriptor *descriptor,
		wimp_w parent, theme_style style, toolbar_flags bar_flags,
		const struct toolbar_callbacks *callbacks, void *client_data,
		const char *help)
{
	struct toolbar *toolbar;

	/* Allocate memory for the bar and link it into the list of bars. */

	toolbar = calloc(sizeof(struct toolbar), 1);
	if (toolbar == NULL) {
		LOG(("No memory for malloc()"));
		warn_user("NoMemory", 0);
		return NULL;
	}

	toolbar->next = ro_toolbar_bars;
	ro_toolbar_bars = toolbar;

	/* Store the supplied settings. */

	toolbar->flags = bar_flags;
	toolbar->theme = descriptor;
	toolbar->style = style;
	toolbar->parent_handle = parent;
	toolbar->callbacks = callbacks;
	toolbar->client_data = client_data;

	/* Set up the internal widgets: initially, there are none. */

	toolbar->buttons = NULL;
	toolbar->buttons_display = false;

	toolbar->url = NULL;
	toolbar->url_display = false;

	toolbar->throbber = NULL;
	toolbar->throbber_display = false;

	/* Set up the bar editor. */

	toolbar->editor = NULL;
	toolbar->editor_div1 = -1;

	toolbar->editing = false;

	toolbar->help_prefix = help;

	return toolbar;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_add_buttons(struct toolbar *toolbar,
		const struct button_bar_buttons buttons[], char *button_order)
{
	if (toolbar == NULL)
		return false;

	if (toolbar->buttons != NULL)
		return false;

	toolbar->buttons = ro_gui_button_bar_create(toolbar->theme, buttons);
	if (toolbar->buttons != NULL) {
		toolbar->buttons_display = true;
		ro_gui_button_bar_arrange_buttons(toolbar->buttons,
				button_order);
	}

	toolbar->editor = ro_gui_button_bar_create(toolbar->theme, buttons);
	if (toolbar->editor != NULL)
		ro_gui_button_bar_hide(toolbar->editor, !toolbar->editing);

	if (toolbar->buttons != NULL && toolbar->editor != NULL)
		if (!ro_gui_button_bar_link_editor(toolbar->buttons,
				toolbar->editor,
				(void (*)(void *))
					ro_toolbar_update_current_widgets,
				toolbar))
			return false;

	return (toolbar->buttons == NULL || toolbar->editor == NULL) ?
			false : true;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_add_throbber(struct toolbar *toolbar)
{
	if (toolbar == NULL)
		return false;

	if (toolbar->throbber != NULL)
		return false;

	toolbar->throbber = ro_gui_throbber_create(toolbar->theme);

	if (toolbar->throbber != NULL)
		toolbar->throbber_display = true;

	return (toolbar->throbber == NULL) ? false : true;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_add_url(struct toolbar *toolbar)
{
	if (toolbar == NULL)
		return false;

	if (toolbar->url != NULL)
		return false;

	toolbar->url = ro_gui_url_bar_create(toolbar->theme);

	if (toolbar->url != NULL)
		toolbar->url_display = true;

	return (toolbar->url == NULL) ? false : true;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_rebuild(struct toolbar *toolbar)
{
	os_error		*error;
	wimp_icon_create	icon;
	wimp_w			old_window = NULL;

	if (toolbar == NULL)
		return false;

	/* Start to set up the toolbar window. */

	ro_toolbar_window.sprite_area =
			ro_gui_theme_get_sprites(toolbar->theme);
	ro_toolbar_window.work_bg =
			ro_gui_theme_get_style_element(toolbar->theme,
			toolbar->style, THEME_ELEMENT_BACKGROUND);

	/* Delete any existing toolbar window... */

	if (toolbar->toolbar_handle != NULL) {
		old_window = toolbar->toolbar_handle;
		error = xwimp_delete_window(toolbar->toolbar_handle);
		if (error)
			LOG(("xwimp_delete_window: 0x%x: %s",
					error->errnum, error->errmess));
		toolbar->toolbar_handle = NULL;
	}

	/* ...and create a new window. */

	error = xwimp_create_window(&ro_toolbar_window,
			&toolbar->toolbar_handle);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* Set up the toolbar's event handlers.  Only set the user activity-
	 * related callbacks if the bar isn't for display purposes.  If the
	 * toolbar is being recreated, simply transfer the handlers across
	 * from the old, now-deleted window.
	 */

	if (old_window == NULL) {
		ro_gui_wimp_event_register_redraw_window(
				toolbar->toolbar_handle, ro_toolbar_redraw);
		ro_gui_wimp_event_set_user_data(toolbar->toolbar_handle,
				toolbar);

		if (!(toolbar->flags & TOOLBAR_FLAGS_DISPLAY)) {
			ro_gui_wimp_event_register_mouse_click(
					toolbar->toolbar_handle,
					ro_toolbar_click);
			ro_gui_wimp_event_register_keypress(
					toolbar->toolbar_handle,
					ro_toolbar_keypress);
			ro_gui_wimp_event_register_menu_prepare(
					toolbar->toolbar_handle,
					ro_toolbar_menu_prepare);
			ro_gui_wimp_event_register_menu_warning(
					toolbar->toolbar_handle,
					ro_toolbar_menu_warning);
			ro_gui_wimp_event_register_menu_selection(
					toolbar->toolbar_handle,
					ro_toolbar_menu_select);
			ro_gui_wimp_event_register_menu(toolbar->toolbar_handle,
					toolbar_menu, true, false);
			ro_gui_wimp_event_register_help_suffix(
					toolbar->toolbar_handle,
					ro_toolbar_get_help_suffix);
		}
	} else {
		ro_gui_wimp_event_transfer(old_window, toolbar->toolbar_handle);
	}

	/* The help prefix changes from edit to non-edit more. */

	ro_gui_wimp_event_set_help_prefix(toolbar->toolbar_handle,
			(toolbar->editing) ?
				"HelpEditToolbar" : toolbar->help_prefix);

	/* Place the widgets into the new bar, using the new theme.
	 *
	 * \TODO -- If any widgets fail to rebuild, then we currently just
	 *          carry on without them.  Not sure if the whole bar
	 *          rebuild should fail here?
	 */

	if (toolbar->throbber != NULL) {
		if (!ro_gui_throbber_rebuild(toolbar->throbber, toolbar->theme,
				toolbar->style, toolbar->toolbar_handle,
				toolbar->editing)) {
			ro_gui_throbber_destroy(toolbar->throbber);
			toolbar->throbber = NULL;
		}

		ro_gui_theme_get_throbber_data(toolbar->theme, NULL, NULL, NULL,
				&toolbar->throbber_right, NULL);
	}

	if (toolbar->buttons != NULL) {
		if (!ro_gui_button_bar_rebuild(toolbar->buttons, toolbar->theme,
				toolbar->style, toolbar->toolbar_handle,
				toolbar->editing)) {
			ro_gui_button_bar_destroy(toolbar->buttons);
			toolbar->buttons = NULL;
		}
	}

	if (toolbar->editor != NULL) {
		if (!ro_gui_button_bar_rebuild(toolbar->editor, toolbar->theme,
				toolbar->style, toolbar->toolbar_handle,
				toolbar->editing)) {
			ro_gui_button_bar_destroy(toolbar->editor);
			toolbar->editor = NULL;
		}
	}

	if (toolbar->url != NULL) {
		if (!ro_gui_url_bar_rebuild(toolbar->url, toolbar->theme,
				toolbar->style, toolbar->toolbar_handle,
				toolbar->flags & TOOLBAR_FLAGS_DISPLAY,
				toolbar->editing)) {
			ro_gui_url_bar_destroy(toolbar->url);
			toolbar->url = NULL;
		}
	}

	/* If this is an editor, add in a divider icon and the editor
	 * button bar.
	 */

	if (toolbar->editing) {
		icon.w = toolbar->toolbar_handle;
		icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
				wimp_ICON_VCENTRED | wimp_ICON_BORDER |
				(wimp_COLOUR_BLACK <<
					wimp_ICON_FG_COLOUR_SHIFT) |
				(wimp_COLOUR_VERY_LIGHT_GREY <<
					wimp_ICON_BG_COLOUR_SHIFT);
		icon.icon.extent.x0 = 0;
		icon.icon.extent.x1 = 0;
		icon.icon.extent.y1 = 0;
		icon.icon.extent.y0 = 0;
		icon.icon.data.indirected_text.text = ro_toolbar_null_string;
		icon.icon.data.indirected_text.validation =
				ro_toolbar_line_validation;
		icon.icon.data.indirected_text.size = 1;
		error = xwimp_create_icon(&icon, &toolbar->editor_div1);
		if (error) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			toolbar->editor_div1 = -1;
		}
	}

	/* Establish the required dimensions to fit the widgets, then
	 * reflow the bar contents.
	 */

	ro_toolbar_refresh_widget_dimensions(toolbar);

	ro_toolbar_process(toolbar, -1, true);

	if (toolbar->parent_handle != NULL)
		ro_toolbar_attach(toolbar, toolbar->parent_handle);

	ro_toolbar_update_buttons(toolbar);

	return true;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_attach(struct toolbar *toolbar, wimp_w parent)
{
	wimp_outline		outline;
	wimp_window_state	state;
	os_error		*error;

	if (toolbar == NULL || toolbar->toolbar_handle == NULL)
		return false;

	toolbar->parent_handle = parent;

	/* Only try to attach the toolbar if there's any of it visible to
	 * matter.
	 */

	if (toolbar->current_height > 0) {
		outline.w = parent;
		xwimp_get_window_outline(&outline);
		state.w = parent;
		xwimp_get_window_state(&state);
		state.w = toolbar->toolbar_handle;
		state.visible.x1 = outline.outline.x1 - 2;
		state.visible.y0 = state.visible.y1 + 2 -
				toolbar->current_height;
		state.xscroll = 0;
		state.yscroll = 0;
		error = xwimp_open_window_nested(PTR_WIMP_OPEN(&state), parent,
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_XORIGIN_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
						<< wimp_CHILD_YORIGIN_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_LS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
						<< wimp_CHILD_BS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
						<< wimp_CHILD_RS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
						<< wimp_CHILD_TS_EDGE_SHIFT);
		if (error) {
			LOG(("xwimp_open_window_nested: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		return true;
	}

	error = xwimp_close_window(toolbar->toolbar_handle);
	if (error) {
		LOG(("xwimp_close_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	return true;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_process(struct toolbar *toolbar, int width, bool reformat)
{
	os_error		*error;
	wimp_outline		outline;
	wimp_window_state	state;
	os_box			extent;
	int			old_height, old_width;
	int			xeig, yeig;
	os_coord		pixel = {1, 1};

	if (!toolbar)
		return false;

	old_height = toolbar->current_height;
	old_width = toolbar->current_width;

	/* calculate 1px in OS units */

	ro_convert_pixels_to_os_units(&pixel, (os_mode)-1);
	xeig = pixel.x;
	yeig = pixel.y;

	/* Measure the parent window width if the caller has asked us to
	 * calculate the clip width ourselves.  Otherwise, if a clip width
	 * has been specified, set the clip to that.
	 */

	if ((toolbar->parent_handle != NULL) && (width == -1)) {
		outline.w = toolbar->parent_handle;
		error = xwimp_get_window_outline(&outline);
		if (error) {
			LOG(("xwimp_get_window_outline: 0x%x: %s",
				error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		toolbar->clip_width = outline.outline.x1 -
				outline.outline.x0 - 2;
		toolbar->current_width = toolbar->clip_width;
	} else if (width != -1) {
		toolbar->clip_width = width;
		toolbar->current_width = toolbar->clip_width;
	}

	/* Find the parent visible height to clip our toolbar height to
	 */

	if (toolbar->parent_handle != NULL) {
		state.w = toolbar->parent_handle;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		toolbar->clip_height = state.visible.y1 - state.visible.y0 + 2;

		/* We can't obscure the height of the scroll bar as we
		 * lose the resize icon if we do.
		 */

		if (toolbar->clip_height >= toolbar->full_height)
			toolbar->current_height = toolbar->full_height;
		else
			toolbar->current_height = toolbar->clip_height;

		/* Resize the work area extent and update our position. */

		if (old_height != toolbar->current_height) {
			extent.x0 = 0;
			extent.y0 = 0;
			extent.x1 = TOOLBAR_DEFAULT_WIDTH;
			extent.y1 = toolbar->current_height - 2;
			error = xwimp_set_extent(toolbar->toolbar_handle,
					&extent);
			if (error) {
				LOG(("xwimp_get_window_state: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
			}

			ro_toolbar_attach(toolbar, toolbar->parent_handle);
		}
	} else {
		toolbar->clip_height = toolbar->full_height;
		toolbar->current_height = toolbar->full_height;
	}

	/* Reflow the widgets into the toolbar if the dimensions have
	 * changed or we have been asked to anyway. */

	if (toolbar->current_width != old_width || reformat)
		ro_toolbar_reformat_widgets(toolbar);

	return true;
}


/**
 * Update the widgets currently on view in a toolbar.  This can be used
 * generally, but is primarily offered to widgets as a way for them
 * to force an update.
 *
 * \param *toolbar		The toolbar to update.
 */

void ro_toolbar_update_current_widgets(struct toolbar *toolbar)
{
	int		old_height;

	if (toolbar == NULL)
		return;

	old_height = toolbar->full_height;

	ro_toolbar_refresh_widget_dimensions(toolbar);
	ro_toolbar_reformat_widgets(toolbar);

	/* If the toolbar height has changed, we need to tell the client. */

	if (toolbar->full_height != old_height)
		ro_toolbar_refresh(toolbar);
}


/**
 * Get the minimum dimenstions required by the toolbar widgets after
 * these have changed.  The minimum dimensions are assumed not to change
 * unless we change theme (ie. we rebuild the bar) or we knowingly
 * alter a widget (eg. we add or remove button-bar buttons).
 *
 *
 * \param *toolbar		The toolbar to refresh.
 */

void ro_toolbar_refresh_widget_dimensions(struct toolbar *toolbar)
{
	int		width, height;
	int		row_width, row_height;

	if (toolbar == NULL)
		return;

	/* Process the toolbar editor and any associated divider rows.
	 */

	if (toolbar->editor != NULL && toolbar->editing) {
		width = 0;
		height = 0;
		ro_gui_button_bar_get_dims(toolbar->editor, &width, &height);

		toolbar->editor_size.x = width;
		toolbar->editor_size.y = height;

		toolbar->row_y0[TOOLBAR_ROW_EDIT] = TOOLBAR_WIDGET_GUTTER;
		toolbar->row_y1[TOOLBAR_ROW_EDIT] = TOOLBAR_WIDGET_GUTTER
				+ height;

		toolbar->row_y0[TOOLBAR_ROW_DIV1] = TOOLBAR_WIDGET_GUTTER +
				toolbar->row_y1[TOOLBAR_ROW_EDIT];
		toolbar->row_y1[TOOLBAR_ROW_DIV1] = 8 +
				toolbar->row_y0[TOOLBAR_ROW_DIV1];
	} else {
		toolbar->editor_size.x = 0;
		toolbar->editor_size.y = 0;

		toolbar->row_y0[TOOLBAR_ROW_EDIT] = 0;
		toolbar->row_y1[TOOLBAR_ROW_EDIT] = 0;
		toolbar->row_y0[TOOLBAR_ROW_DIV1] = 0;
		toolbar->row_y1[TOOLBAR_ROW_DIV1] = 0;
	}

	/* Process the top row icons. */

	row_width = 0;
	row_height = 0;

	/* If the editor is active, any button bar if forced into view. */

	if (toolbar->buttons != NULL &&
			(toolbar->buttons_display || toolbar->editing)) {
		width = 0;
		height = 0;
		ro_gui_button_bar_get_dims(toolbar->buttons, &width, &height);

		row_width += width;
		toolbar->buttons_size.x = width;
		toolbar->buttons_size.y = height;

		if (height > row_height)
			row_height = height;
	} else {
		toolbar->buttons_size.x = 0;
		toolbar->buttons_size.y = 0;
	}

	if (toolbar->url != NULL && toolbar->url_display) {
		width = 0;
		height = 0;
		ro_gui_url_bar_get_dims(toolbar->url, &width, &height);

		if (row_width > 0)
			row_width += TOOLBAR_WIDGET_GUTTER;
		row_width += width;

		toolbar->url_size.x = width;
		toolbar->url_size.y = height;

		if (height > row_height)
			row_height = height;
	} else {
		toolbar->url_size.x = 0;
		toolbar->url_size.y = 0;
	}

	if (toolbar->throbber != NULL && toolbar->throbber_display) {
		width = 0;
		height = 0;
		ro_gui_throbber_get_dims(toolbar->throbber, &width, &height);

		if (row_width > 0)
			row_width += TOOLBAR_WIDGET_GUTTER;
		row_width += width;

		toolbar->throbber_size.x = width;
		toolbar->throbber_size.y = height;

		if (height > row_height)
			row_height = height;
	} else {
		toolbar->throbber_size.x = 0;
		toolbar->throbber_size.y = 0;
	}

	if (row_height > 0) {
		toolbar->row_y0[TOOLBAR_ROW_TOP] = TOOLBAR_WIDGET_GUTTER +
				toolbar->row_y1[TOOLBAR_ROW_DIV1];
		toolbar->row_y1[TOOLBAR_ROW_TOP] = row_height +
				toolbar->row_y0[TOOLBAR_ROW_TOP];
	} else {
		toolbar->row_y0[TOOLBAR_ROW_TOP] = 0;
		toolbar->row_y1[TOOLBAR_ROW_TOP] = 0;
	}

	/* Establish the full dimensions of the bar.
	 *
	 * \TODO -- This currently assumes an "all or nothing" approach to
	 *          the editor bar, and will need reworking once we have to
	 *          worry about tab bars.
	 */

	if (toolbar->row_y1[TOOLBAR_ROW_TOP] > 0) {
		toolbar->full_height = toolbar->row_y1[TOOLBAR_ROW_TOP] +
				TOOLBAR_WIDGET_GUTTER;
	} else {
		toolbar->full_height = 0;
	}
	toolbar->full_width = 2 * TOOLBAR_WIDGET_GUTTER +
			(row_width > toolbar->editor_size.x) ?
				row_width : toolbar->editor_size.x;
}


/**
 * Reformat (reflow) the widgets into the toolbar, based on the toolbar size
 * and the previously calculated widget dimensions.
 *
 * \param *toolbar		The toolbar to reformat.
 */

void ro_toolbar_reformat_widgets(struct toolbar *toolbar)
{
	int		left_margin, right_margin;

	left_margin = TOOLBAR_WIDGET_GUTTER;
	right_margin = toolbar->clip_width - TOOLBAR_WIDGET_GUTTER;

	/* Flow the toolbar editor row, which will be a fixed with and
	 * may alter the right margin.
	 */

	if (toolbar->editor != NULL && toolbar->editing) {
		if (right_margin < left_margin + toolbar->editor_size.x)
			right_margin = left_margin + toolbar->editor_size.x;

		ro_gui_button_bar_set_extent(toolbar->editor,
				left_margin,
				toolbar->row_y0[TOOLBAR_ROW_EDIT],
				left_margin + toolbar->editor_size.x,
				toolbar->row_y1[TOOLBAR_ROW_EDIT]);

		if (toolbar->editor_div1 != -1)
			xwimp_resize_icon(toolbar->toolbar_handle,
					toolbar->editor_div1, -8,
					toolbar->row_y0[TOOLBAR_ROW_DIV1],
					toolbar->clip_width + 8,
					toolbar->row_y1[TOOLBAR_ROW_DIV1]);
	}

	/* Flow the top row. */

	if (toolbar->throbber != NULL && toolbar->throbber_display) {
		if (toolbar->throbber_right) {
			right_margin -= (toolbar->throbber_size.x +
					TOOLBAR_WIDGET_GUTTER);
		} else {
			ro_gui_throbber_set_extent(toolbar->throbber,
					left_margin,
					toolbar->row_y0[TOOLBAR_ROW_TOP],
					left_margin + toolbar->throbber_size.x,
					toolbar->row_y1[TOOLBAR_ROW_TOP]);
			left_margin += (toolbar->throbber_size.x +
					TOOLBAR_WIDGET_GUTTER);
		}
	}

	if (toolbar->buttons != NULL &&
			(toolbar->buttons_display || toolbar->editing)) {
		if (right_margin < left_margin + toolbar->buttons_size.x)
			right_margin = left_margin + toolbar->buttons_size.x;

		ro_gui_button_bar_set_extent(toolbar->buttons,
				left_margin,
				toolbar->row_y0[TOOLBAR_ROW_TOP],
				left_margin + toolbar->buttons_size.x,
				toolbar->row_y1[TOOLBAR_ROW_TOP]);
		left_margin += (toolbar->buttons_size.x +
				TOOLBAR_WIDGET_GUTTER);
	}

	if (toolbar->url != NULL && toolbar->url_display) {
		if (right_margin < left_margin + toolbar->url_size.x)
			right_margin = left_margin + toolbar->url_size.x;

		ro_gui_url_bar_set_extent(toolbar->url,
				left_margin,
				toolbar->row_y0[TOOLBAR_ROW_TOP],
				right_margin,
				toolbar->row_y1[TOOLBAR_ROW_TOP]);

		left_margin = right_margin + TOOLBAR_WIDGET_GUTTER;
	}

	if (toolbar->throbber != NULL && toolbar->throbber_display &&
			toolbar->throbber_right) {
		left_margin = right_margin + TOOLBAR_WIDGET_GUTTER;
		ro_gui_throbber_set_extent(toolbar->throbber,
				left_margin,
				toolbar->row_y0[TOOLBAR_ROW_TOP],
				left_margin + toolbar->throbber_size.x,
				toolbar->row_y1[TOOLBAR_ROW_TOP]);
	}

}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_destroy(struct toolbar *toolbar)
{
	struct toolbar *bar;

	if (toolbar == NULL)
		return;

	LOG(("Destroying toolbar 0x%x", (unsigned int) toolbar));

	/* Destroy the widgets. */

	if (toolbar->buttons != NULL)
		ro_gui_button_bar_destroy(toolbar->buttons);

	if (toolbar->editor != NULL)
		ro_gui_button_bar_destroy(toolbar->editor);

	if (toolbar->url != NULL)
		ro_gui_url_bar_destroy(toolbar->url);

	if (toolbar->throbber != NULL)
		ro_gui_throbber_destroy(toolbar->throbber);

	/* Delete the toolbar window. */

	if (toolbar->toolbar_handle != NULL) {
		xwimp_delete_window(toolbar->toolbar_handle);
		ro_gui_wimp_event_finalise(toolbar->toolbar_handle);
	}

	/* Remove the bar from the list and free the memory.
	 */

	if (ro_toolbar_bars == toolbar) {
		ro_toolbar_bars = toolbar->next;
	} else {
		for (bar = ro_toolbar_bars; bar != NULL && bar->next != toolbar;
				bar = bar->next);

		if (bar->next == toolbar)
			bar->next = toolbar->next;
	}

	free(toolbar);

}


/**
 * Handle redraw request events for a toolbar workarea.
 *
 * \param *redraw		The redraw block for the event.
 */

void ro_toolbar_redraw(wimp_draw *redraw)
{
	struct toolbar *toolbar;
	osbool more;
	os_error *error;

	toolbar = (struct toolbar *)ro_gui_wimp_event_get_user_data(redraw->w);

	assert(toolbar != NULL);

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		ro_plot_origin_x = redraw->box.x0 - redraw->xscroll;
		ro_plot_origin_y = redraw->box.y1 - redraw->yscroll;

		if (toolbar->buttons != NULL && toolbar->buttons_display)
			ro_gui_button_bar_redraw(toolbar->buttons, redraw);

		if (toolbar->editor != NULL && toolbar->editing)
			ro_gui_button_bar_redraw(toolbar->editor, redraw);

		if (toolbar->url != NULL && toolbar->url_display)
			ro_gui_url_bar_redraw(toolbar->url, redraw);

		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}


/**
 * Process clicks on a toolbar, passing details on to clients where necessary.
 *
 * \param *pointer		The wimp mouse click event.
 * \return			True if the event was handled; else false.
 */

bool ro_toolbar_click(wimp_pointer *pointer)
{
	struct toolbar		*toolbar;
	union toolbar_action	action;
	wimp_window_state	state;
	os_error		*error;

	toolbar = (struct toolbar *)
			ro_gui_wimp_event_get_user_data(pointer->w);

	if (toolbar == NULL)
		return false;

	assert(pointer->w == toolbar->toolbar_handle);

	state.w = toolbar->toolbar_handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* If the click wasn't in the URL Bar's text field, then it will
	 * need to close any URL Complete window that is open.
	 *
	 * \TODO -- This should really move into the URL Bar module, as
	 *          URL Complete is really an extension to that.
	 */

	if (toolbar->url != NULL && toolbar->url_display &&
			!ro_gui_url_bar_test_for_text_field_click(toolbar->url,
				pointer))
		ro_gui_url_complete_close();

	/* Pass the click around the toolbar widgets. */

	if (toolbar->buttons != NULL &&
			(toolbar->buttons_display || toolbar->editing) &&
			ro_gui_button_bar_click(toolbar->buttons, pointer,
				&state, &action.button)) {
		if (action.button != TOOLBAR_BUTTON_NONE &&
				!toolbar->editing &&
				toolbar->callbacks != NULL &&
				toolbar->callbacks->user_action != NULL)
			toolbar->callbacks->user_action(toolbar->client_data,
					TOOLBAR_ACTION_BUTTON, action);
		return true;
	}

	if (toolbar->url != NULL && toolbar->url_display &&
			ro_gui_url_bar_click(toolbar->url, pointer,
				&state, &action.url)) {
		if (action.url != TOOLBAR_URL_NONE &&
				!toolbar->editing &&
				toolbar->callbacks != NULL &&
				toolbar->callbacks->user_action != NULL)
			toolbar->callbacks->user_action(toolbar->client_data,
					TOOLBAR_ACTION_URL, action);
		return true;
	}

	if (toolbar->editor != NULL && toolbar->editing &&
			ro_gui_button_bar_click(toolbar->editor, pointer,
				&state, &action.button)) {
		return true;
	}

	/* Nothing else has handled this, so try passing it to the
	 * URL Complete module.
	 *
	 * \TODO -- This should really move into the URL Bar module, as
	 *          URL Complete is really an extension to that.
	 */

	if (toolbar->url != NULL && toolbar->url_display &&
			ro_gui_url_bar_test_for_text_field_click(toolbar->url,
				pointer)) {
		ro_gui_url_complete_start(toolbar);
		return true;
	}

	return false;
}


/**
 * Process keypresses in a toolbar, passing details on to clients where
 * necessary.
 *
 * \param *key			The wimp key press event.
 * \return			True if the event was handled; else false.
 */

bool ro_toolbar_keypress(wimp_key *key)
{
	struct toolbar		*toolbar;

	toolbar = (struct toolbar *) ro_gui_wimp_event_get_user_data(key->w);

	if (toolbar == NULL)
		return false;

	/* Pass the keypress on to the client and stop if they handle it. */

	if (toolbar->callbacks->key_press != NULL &&
			toolbar->callbacks->key_press(toolbar->client_data, key))
		return true;

	/* If the caret is in the URL bar, ask the URL Complete module if it
	 * wants to handle the keypress.
	 *
	 * \TODO -- This should really move into the URL Bar module, as
	 *          URL Complete is really an extension to that.
	 */

	if (toolbar->url != NULL && toolbar->url_display &&
			ro_gui_url_bar_test_for_text_field_keypress(
				toolbar->url, key) &&
			ro_gui_url_complete_keypress(toolbar, key->c))
		return true;

	return false;
}


/**
 * Prepare the toolbar menu for (re-)opening
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu about to be opened.
 * \param  *pointer		Pointer to the relevant wimp event block, or
 *				NULL for an Adjust click.
 * \return			true if the event was handled; else false.
 */

bool ro_toolbar_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer)
{
	struct toolbar		*toolbar;

	toolbar = (struct toolbar *) ro_gui_wimp_event_get_user_data(w);

	if (toolbar == NULL)
		return false;

	/* Pass the event on to potentially interested widgets. */

	if (toolbar->url != NULL && ro_gui_url_bar_menu_prepare(toolbar->url,
			i, menu, pointer))
		return true;

	/* Try to process the event as a toolbar menu. */

	if (menu != toolbar_menu)
		return false;

	/* Shade menu entries according to the state of the window and object
	 * under the pointer.
	 */

	/* Toolbar (Sub)Menu */

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_shade(toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_tick(toolbar));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_option_shade(toolbar) ||
			toolbar->buttons == NULL);
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_buttons_tick(toolbar) &&
			toolbar->buttons != NULL);

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_ADDRESS_BAR,
			ro_toolbar_menu_edit_shade(toolbar) ||
			toolbar->url == NULL);
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_ADDRESS_BAR,
			ro_toolbar_menu_url_tick(toolbar) &&
			toolbar->url != NULL);

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_THROBBER,
			ro_toolbar_menu_edit_shade(toolbar) ||
			toolbar->throbber == NULL);
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_THROBBER,
			ro_toolbar_menu_throbber_tick(toolbar) &&
			toolbar->throbber != NULL);

	return true;
}


/**
 * Handle submenu warnings for the toolbar menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_toolbar_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	/* Do nothing */
}


/**
 * Handle selections from the toolbar menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu from which the selection was made.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_toolbar_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	struct toolbar		*toolbar;

	toolbar = (struct toolbar *) ro_gui_wimp_event_get_user_data(w);

	if (toolbar == NULL)
		return false;

	/* Pass the event on to potentially interested widgets. */

	if (toolbar->url != NULL && ro_gui_url_bar_menu_select(toolbar->url,
			i, menu, selection, action))
		return true;

	/* Try to process the event as a toolbar menu. */

	if (menu != toolbar_menu)
		return false;

	switch (action) {
	case TOOLBAR_BUTTONS:
		ro_toolbar_set_display_buttons(toolbar,
				!ro_toolbar_get_display_buttons(toolbar));
		break;
	case TOOLBAR_ADDRESS_BAR:
		ro_toolbar_set_display_url(toolbar,
				!ro_toolbar_get_display_url(toolbar));
		if (ro_toolbar_get_display_url(toolbar))
			ro_toolbar_take_caret(toolbar);
		break;
	case TOOLBAR_THROBBER:
		ro_toolbar_set_display_throbber(toolbar,
				!ro_toolbar_get_display_throbber(toolbar));
		break;
	case TOOLBAR_EDIT:
		ro_toolbar_toggle_edit(toolbar);
		break;
	default:
		return false;
	}

	return true;
}


/**
 * Translate the contents of a message_HELP_REQUEST into a suffix for a
 * NetSurf message token.  The help system will then add this to whatever
 * prefix the current toolbar has registered with WimpEvent.
 *
 * \param w			The window handle under the mouse.
 * \param i			The icon handle under the mouse.
 * \param *pos			The mouse position.
 * \param buttons		The mouse button state.
 * \return			The required help token suffix.
 */

const char *ro_toolbar_get_help_suffix(wimp_w w, wimp_i i, os_coord *pos,
		wimp_mouse_state buttons)
{
	struct toolbar		*toolbar;
	wimp_window_state	state;
	os_error		*error;
	const char		*suffix;

	toolbar = (struct toolbar *) ro_gui_wimp_event_get_user_data(w);

	if (toolbar == NULL || toolbar->toolbar_handle != w)
		return NULL;

	state.w = toolbar->toolbar_handle;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return NULL;
	}

	/* Pass the help request around the toolbar widgets. */

	if (toolbar->throbber != NULL && toolbar->throbber_display &&
			ro_gui_throbber_help_suffix(toolbar->throbber, i,
			pos, &state, buttons, &suffix))
		return suffix;

	if (toolbar->url != NULL && toolbar->url_display &&
			ro_gui_url_bar_help_suffix(toolbar->url, i,
			pos, &state, buttons, &suffix))
		return suffix;

	if (toolbar->buttons != NULL && toolbar->buttons_display &&
			ro_gui_button_bar_help_suffix(toolbar->buttons, i,
			pos, &state, buttons, &suffix))
		return suffix;

	return "";
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_update_client_data(struct toolbar *toolbar, void *client_data)
{
	if (toolbar != NULL)
		toolbar->client_data = client_data;
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_update_all_buttons(void)
{
	struct toolbar *bar;

	bar = ro_toolbar_bars;
	while (bar != NULL) {
		ro_toolbar_update_buttons(bar);

		bar = bar->next;
	}
}


/**
 * Update the state of a toolbar's buttons.
 *
 * \param toolbar  the toolbar to update
 */

void ro_toolbar_update_buttons(struct toolbar *toolbar)
{
	assert(toolbar != NULL);

	if (toolbar->callbacks != NULL &&
			toolbar->callbacks->update_buttons != NULL)
		toolbar->callbacks->update_buttons(toolbar->client_data);
}

/* This is an exported interface documented in toolbar.h */

void ro_toolbar_refresh(struct toolbar *toolbar)
{
	assert(toolbar != NULL);

	ro_toolbar_process(toolbar, -1, true);
	if (toolbar->callbacks != NULL &&
			toolbar->callbacks->change_size != NULL)
		toolbar->callbacks->change_size(toolbar->client_data);

	if (toolbar->toolbar_handle != NULL)
		xwimp_force_redraw(toolbar->toolbar_handle, 0, 0,
				toolbar->current_width,
				toolbar->current_height);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_theme_update(void)
{
	struct toolbar	*bar, *next;
	bool		ok;

	bar = ro_toolbar_bars;
	while (bar != NULL) {
		/* Take the next bar address now, as *bar may become invalid
		 * during the update process (if an update fails and
		 * ro_toolbar_destroy() is called) and we don't want to lose
		 * the link to the rest of the chain.
		 */

		next = bar->next;

		/* Only process the bar if the theme is set to the default.
		 * Otherwise, it's up to the owner to do whatever they need
		 * to do for themselves.
		 */

		if (bar->theme == NULL) {
			ok = ro_toolbar_rebuild(bar);

			if (!ok)
				ro_toolbar_destroy(bar);
		} else {
			ok = true;
		}

		if (bar->callbacks != NULL &&
				bar->callbacks->theme_update != NULL)
			bar->callbacks->theme_update(bar->client_data, ok);

		bar = next;
	}
}


/* This is an exported interface documented in toolbar.h */

struct toolbar *ro_toolbar_parent_window_lookup(wimp_w w)
{
	struct toolbar *toolbar;

	toolbar = ro_toolbar_bars;
	while (toolbar != NULL && toolbar->parent_handle != w)
		toolbar = toolbar->next;

	return toolbar;
}


/* This is an exported interface documented in toolbar.h */

struct toolbar *ro_toolbar_window_lookup(wimp_w w)
{
	struct toolbar *toolbar;

	toolbar = ro_toolbar_bars;
	while (toolbar != NULL && toolbar->toolbar_handle != w)
		toolbar = toolbar->next;

	return toolbar;
}


/* This is an exported interface documented in toolbar.h */

wimp_w ro_toolbar_get_parent_window(struct toolbar *toolbar)
{
	return (toolbar != NULL) ? toolbar->parent_handle : 0;
}


/* This is an exported interface documented in toolbar.h */

wimp_w ro_toolbar_get_window(struct toolbar *toolbar)
{
	return (toolbar != NULL) ? toolbar->toolbar_handle : 0;
}


/* This is an exported interface documented in toolbar.h */

int ro_toolbar_height(struct toolbar *toolbar)
{
	return (toolbar == NULL) ? 0 : toolbar->current_height;
}


/* This is an exported interface documented in toolbar.h */

int ro_toolbar_full_height(struct toolbar *toolbar)
{
	return (toolbar == NULL) ? 0 : toolbar->full_height;
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_start_throbbing(struct toolbar *toolbar)
{
	if (toolbar != NULL && toolbar->throbber != NULL)
		ro_gui_throbber_animate(toolbar->throbber);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_stop_throbbing(struct toolbar *toolbar)
{
	if (toolbar != NULL && toolbar->throbber != NULL)
		ro_gui_throbber_stop(toolbar->throbber);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_throb(struct toolbar *toolbar)
{
	if (toolbar != NULL && toolbar->throbber != NULL)
		ro_gui_throbber_animate(toolbar->throbber);
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_set_button_order(struct toolbar *toolbar, char order[])
{
	if (toolbar == NULL || toolbar->buttons == NULL)
		return false;

	if (!ro_gui_button_bar_arrange_buttons(toolbar->buttons, order))
		return false;

	ro_toolbar_refresh_widget_dimensions(toolbar);

	return ro_toolbar_process(toolbar, -1, true);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_set_button_shaded_state(struct toolbar *toolbar,
		button_bar_action action, bool shaded)
{
	if (toolbar == NULL || toolbar->buttons == NULL)
		return;

	ro_gui_button_bar_shade_button(toolbar->buttons, action, shaded);
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_take_caret(struct toolbar *toolbar)
{
	if (toolbar == NULL || toolbar->url == NULL || !toolbar->url_display)
		return false;

	return ro_gui_url_bar_take_caret(toolbar->url);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_set_url(struct toolbar *toolbar, const char *url,
		bool is_utf8, bool set_caret)
{
	if (toolbar != NULL && toolbar->url != NULL)
		ro_gui_url_bar_set_url(toolbar->url, url, is_utf8, set_caret);
}


/* This is an exported interface documented in toolbar.h */

const char *ro_toolbar_get_url(struct toolbar *toolbar)
{
	if (toolbar == NULL || toolbar->url == NULL)
		return NULL;

	return ro_gui_url_bar_get_url(toolbar->url);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_update_all_hotlists(void)
{
	struct toolbar *bar;

	bar = ro_toolbar_bars;
	while (bar != NULL) {
		ro_toolbar_update_hotlist(bar);

		bar = bar->next;
	}
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_update_hotlist(struct toolbar *toolbar)
{
	if (toolbar == NULL || toolbar->url == NULL)
		return;
	
	ro_gui_url_bar_update_hotlist(toolbar->url);
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_get_url_field_extent(struct toolbar *toolbar, os_box *extent)
{
	if (toolbar == NULL || toolbar->url == NULL)
		return false;

	if (extent == NULL)
		return true;

	return ro_gui_url_bar_get_url_extent(toolbar->url, extent);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_set_site_favicon(struct toolbar *toolbar,
		struct hlcache_handle *h)
{
	if (toolbar == NULL || toolbar->url == NULL)
		return;

	ro_gui_url_bar_set_site_favicon(toolbar->url, h);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_set_content_favicon(struct toolbar *toolbar,
		struct hlcache_handle *h)
{
	if (toolbar == NULL || toolbar->url == NULL)
		return;

	ro_gui_url_bar_set_content_favicon(toolbar->url, h);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_update_urlsuggest(struct toolbar *toolbar)
{
	if (toolbar == NULL || toolbar->url == NULL)
		return;

	ro_gui_url_bar_update_urlsuggest(toolbar->url);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_set_display_buttons(struct toolbar *toolbar, bool display)
{
	if (toolbar == NULL || toolbar->buttons == NULL)
		return;

	toolbar->buttons_display = display;
	ro_gui_button_bar_hide(toolbar->buttons, !display);
	ro_toolbar_refresh_widget_dimensions(toolbar);
	ro_toolbar_refresh(toolbar);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_set_display_url(struct toolbar *toolbar, bool display)
{
	if (toolbar == NULL || toolbar->url == NULL)
		return;

	toolbar->url_display = display;
	ro_gui_url_bar_hide(toolbar->url, !display);
	ro_toolbar_refresh_widget_dimensions(toolbar);
	ro_toolbar_refresh(toolbar);
}


/* This is an exported interface documented in toolbar.h */

void ro_toolbar_set_display_throbber(struct toolbar *toolbar, bool display)
{
	if (toolbar == NULL || toolbar->throbber == NULL)
		return;

	toolbar->throbber_display = display;
	ro_gui_throbber_hide(toolbar->throbber, !display);
	ro_toolbar_refresh_widget_dimensions(toolbar);
	ro_toolbar_refresh(toolbar);
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_get_display_buttons(struct toolbar *toolbar)
{
	return (toolbar == NULL || toolbar->buttons == NULL) ?
			false : toolbar->buttons_display;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_get_display_url(struct toolbar *toolbar)
{
	return (toolbar == NULL || toolbar->url == NULL) ?
			false : toolbar->url_display;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_get_display_throbber(struct toolbar *toolbar)
{
	return (toolbar == NULL || toolbar->throbber == NULL) ?
			false : toolbar->throbber_display;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_get_editing(struct toolbar *toolbar)
{
	return (toolbar == NULL || !toolbar->editing) ? false : true;
}


/* This is an exported interface documented in toolbar.h */

bool ro_toolbar_toggle_edit(struct toolbar *toolbar)
{
	char		*new_buttons;

	if (toolbar == NULL || toolbar->editor == NULL)
		return false;

	toolbar->editing = !toolbar->editing;

	ro_gui_button_bar_hide(toolbar->editor, !toolbar->editing);
	ro_gui_button_bar_hide(toolbar->buttons,
			!toolbar->buttons_display && !toolbar->editing);

	if (!ro_toolbar_rebuild(toolbar)) {
		ro_toolbar_destroy(toolbar);
		return false;
	}

	ro_toolbar_refresh(toolbar);

	/* If there's a callback registered and an edit has finished,
	 * tell out client what the new button state is.
	 */

	if (!toolbar->editing && toolbar->buttons != NULL &&
			toolbar->callbacks != NULL &&
			toolbar->callbacks->save_buttons != NULL) {
		new_buttons = ro_gui_button_bar_get_config(toolbar->buttons);
		toolbar->callbacks->save_buttons(toolbar->client_data,
				new_buttons);
	}

	return true;
}

