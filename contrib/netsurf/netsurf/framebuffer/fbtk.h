/*
 * Copyright 2008,2010 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef NETSURF_FB_FBTK_H
#define NETSURF_FB_FBTK_H

#ifdef FBTK_LOGGING
#define FBTK_LOG(x) LOG(x)
#else
#define FBTK_LOG(x)
#endif

#define FB_SCROLL_COLOUR 0xFFAAAAAA
#define FB_FRAME_COLOUR 0xFFDDDDDD
#define FB_COLOUR_BLACK 0xFF000000
#define FB_COLOUR_WHITE 0xFFFFFFFF

#define FBTK_WIDGET_PADDING 30 /* percentage of widget size used for padding */ 
#define FBTK_DPI 90 /* screen DPI */

typedef struct fbtk_widget_s fbtk_widget_t;

/* Widget Callback handling */
typedef enum fbtk_callback_type {
	FBTK_CBT_START = 0,
	FBTK_CBT_SCROLLX,
	FBTK_CBT_SCROLLY,
	FBTK_CBT_CLICK,
	FBTK_CBT_INPUT,
	FBTK_CBT_POINTERMOVE,
	FBTK_CBT_POINTERLEAVE,
	FBTK_CBT_POINTERENTER,
	FBTK_CBT_REDRAW,
	FBTK_CBT_DESTROY,
	FBTK_CBT_USER,
	FBTK_CBT_STRIP_FOCUS,
	FBTK_CBT_END,
} fbtk_callback_type;

typedef struct fbtk_callback_info {
	enum fbtk_callback_type type;
	void *context;
	nsfb_event_t *event;
	int x;
	int y;
	char *text;
	fbtk_widget_t *widget;
} fbtk_callback_info;

/* structure for framebuffer toolkit bitmaps  */
struct fbtk_bitmap {
        int width;
        int height;
        uint8_t *pixdata;
        bool opaque;

        /* The following two are only used for cursors */
        int hot_x;
        int hot_y;
};

/* Key modifier status */
typedef enum fbtk_modifier_type {
	FBTK_MOD_CLEAR  = 0,
	FBTK_MOD_LSHIFT = (1 << 0),
	FBTK_MOD_RSHIFT = (1 << 1),
	FBTK_MOD_LCTRL  = (1 << 2),
	FBTK_MOD_RCTRL  = (1 << 3)
} fbtk_modifier_type;

typedef int (*fbtk_callback)(fbtk_widget_t *widget, fbtk_callback_info *cbi);

/* enter pressed on writable icon */
typedef int (*fbtk_enter_t)(void *pw, char *text);


/************************ Core ****************************/


/** Initialise widget toolkit.
 *
 * Initialises widget toolkit against a framebuffer.
 *
 * @param fb The underlying framebuffer.
 * @return The root widget handle.
 */
fbtk_widget_t *fbtk_init(nsfb_t *fb);

/** Retrieve the framebuffer library handle from toolkit widget.
 *
 * @param widget A fbtk widget.
 * @return The underlying framebuffer.
 */
nsfb_t *fbtk_get_nsfb(fbtk_widget_t *widget);

/** Perform any pending widget redraws.
 *
 * @param widget A fbtk widget.
 */
int fbtk_redraw(fbtk_widget_t *widget);

/** Determine if there are any redraws pending for a widget.
 *
 * Mainly used by clients on the root widget to determine if they need
 * to call ::fbtk_redraw
 *
 * @param widget to check.
 */
bool fbtk_get_redraw_pending(fbtk_widget_t *widget);

/** clip a bounding box to a widgets area.
 */
bool fbtk_clip_to_widget(fbtk_widget_t *widget, bbox_t * restrict box);

/** clip one bounding box to another.
 */
bool fbtk_clip_rect(const bbox_t * restrict clip, bbox_t * restrict box);

/***************** Callback processing ********************/

/** Helper function to allow simple calling of callbacks with parameters.
 *
 * @param widget The fbtk widget to post the callback to.
 * @param cbt The type of callback to post
 * @param ... Parameters appropriate for the callback type.
 */
int fbtk_post_callback(fbtk_widget_t *widget, fbtk_callback_type cbt, ...);

/** Set a callback handler.
 *
 * Set a callback handler and the pointer to pass for a widget.
 *
 * @param widget The widget to set the handler for.
 * @param cbt The type of callback to set.
 * @param cb The callback.
 * @param pw The private pointer to pass when calling teh callback.
 * @return The previous callback handler for the type or NULL.
 */
fbtk_callback fbtk_set_handler(fbtk_widget_t *widget, fbtk_callback_type cbt, fbtk_callback cb, void *pw);

/** Get a callback handler.
 */
fbtk_callback fbtk_get_handler(fbtk_widget_t *widget, fbtk_callback_type cbt);


/******************* Event processing **********************/

/** Retrive events from the framebuffer input.
 *
 * Obtain events from the framebuffer input system with a
 * timeout. Some events may be used by the toolkit instead of being
 * returned to the caller.
 *
 * @param root An fbtk widget.
 * @param event an event structure to update.
 * @param timeout The number of miliseconds to wait for an event. 0
 *                means do not wait and -1 means wait foreevr.
 * @return wether \a event has been updated.
 */
bool fbtk_event(fbtk_widget_t *root, nsfb_event_t *event, int timeout);

/** Insert mouse button press into toolkit.
 */
void fbtk_click(fbtk_widget_t *widget, nsfb_event_t *event);

/** Insert input into toolkit.
 */
void fbtk_input(fbtk_widget_t *widget, nsfb_event_t *event);

/** Move pointer.
 *
 * Move the pointer cursor to a given location.
 *
 * @param widget any tookit widget.
 * @parm x movement in horizontal plane.
 * @parm y movement in vertical plane.
 * @parm relative Wheter the /a x and /a y should be considered relative to
 *                current pointer position.
 */
void fbtk_warp_pointer(fbtk_widget_t *widget, int x, int y, bool relative);

/** Toggle pointer grab.
 *
 * Toggles the movement grab for a widget.
 *
 * @param widget The widget trying to grab the movement.
 * @return true if the grab was ok, false if the grab failed (already grabbed).
 */
bool fbtk_tgrab_pointer(fbtk_widget_t *widget);

/** Convert a framebuffer keycode to ucs4.
 *
 * Character mapping between keycode with modifier state and ucs-4.
 */
int fbtk_keycode_to_ucs4(int code, fbtk_modifier_type mods);


/******************* Widget Information **********************/

/** Obtain the widget at a point on screen.
 *
 * @param widget any tookit widget.
 * @parm x location in horizontal plane.
 * @parm y location in vertical plane.
 * @return widget or NULL.
 */
fbtk_widget_t *fbtk_get_widget_at(fbtk_widget_t *widget, int x, int y);

/** Get a widget's absolute horizontal screen co-ordinate.
 *
 * @param widget The widget to inspect.
 * @return The absolute screen co-ordinate.
 */
int fbtk_get_absx(fbtk_widget_t *widget);

/** Get a widget's absolute vertical screen co-ordinate.
 *
 * @param widget The widget to inspect.
 * @return The absolute screen co-ordinate.
 */
int fbtk_get_absy(fbtk_widget_t *widget);

/** Get a widget's width.
 *
 * @param widget The widget to inspect.
 * @return The widget width.
 */
int fbtk_get_width(fbtk_widget_t *widget);

/** Get a widget's height.
 *
 * @param widget The widget to inspect.
 * @return The widget height.
 */
int fbtk_get_height(fbtk_widget_t *widget);

/** Get a widget's bounding box in absolute screen co-ordinates.
 *
 * @param widget The widget to inspect.
 * @param bbox The bounding box structure to update.
 * @return If the \a bbox parameter has been updated.
 */
bool fbtk_get_bbox(fbtk_widget_t *widget, struct nsfb_bbox_s *bbox);

/** Get a widget caret pos, if it owns caret.
 *
 * @param widget  The widget to inspect.
 * @param x       If widget has caret, returns x-coord of caret within widget
 * @param y       If widget has caret, returns y-coord of caret within widget
 * @param height  If widget has caret, returns caret height
 * @return true iff widget has caret
 */
bool fbtk_get_caret(fbtk_widget_t *widget, int *x, int *y, int *height);


/******************* Widget Manipulation **********************/

/** Change the widget's position and size.
 *
 */
bool fbtk_set_pos_and_size(fbtk_widget_t *widget, int x, int y, int width, int height);

/** Set caret owner and position
 *
 * @param widget  widget to give caret to, or ensure caret is released from
 * @param set     true: caret to be set for widget, false: caret to be released
 * @param x       x-coordinate of caret top
 * @param y       y-coordinate of caret top
 * @param height  height of caret
 */
void fbtk_set_caret(fbtk_widget_t *widget, bool set, int x, int y, int height,
		void (*remove_caret)(fbtk_widget_t *widget));

/** Map a widget and request it is redrawn.
 */
int fbtk_set_mapping(fbtk_widget_t *widget, bool mapped);

/** Set the z order of a widget.
 */
int fbtk_set_zorder(fbtk_widget_t *widget, int z);

/** Indicate a widget should be redrawn.
 */
void fbtk_request_redraw(fbtk_widget_t *widget);

/** Destroy a widget and all its descendants.
 *
 * Removes a widget from the hierachy and frees it and all its children.
 *
 * @param widget The widget to destroy.
 * @return 0 on success or -1 on error.
 */
int fbtk_destroy_widget(fbtk_widget_t *widget);



/********************************* Widgets *********************************/


/** Create a window widget.
 *
 * @param parent The parent window or the root widget for a top level window.
 * @param x The x location relative to the parent window.
 * @param y the y location relative to the parent window.
 * @param width The width of the window. 0 indicates parents width should be
 *              used. Negative value indicates parents width less the value
 *              should be used. The width is limited to lie within the parent
 *              window.
 * @param height The height of the window limited in a similar way to the
 *               /a width.
 * @param c The background colour.
 * @return new window widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_window(fbtk_widget_t *parent, int x, int y, int width, int height, colour bg);



/** Create a filled rectangle
 *
 * Create a widget which is a filled rectangle, usually used for backgrounds.
 *
 * @param window The window to add the filled area widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *
fbtk_create_fill(fbtk_widget_t *window, int x, int y, int width, int height, colour c);




/** Create a horizontal scroll widget
 *
 * Create a horizontal scroll widget.
 *
 * @param window The window to add the filled area widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *
fbtk_create_hscroll(fbtk_widget_t *window, int x, int y, int width, int height, colour fg, colour bg, fbtk_callback callback, void *context);

/** Create a vertical scroll widget
 *
 * Create a vertical scroll widget.
 *
 * @param window The window to add the filled area widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *
fbtk_create_vscroll(fbtk_widget_t *window, int x, int y, int width, int height, colour fg, colour bg, fbtk_callback callback, void *context);

bool fbtk_set_scroll_parameters(fbtk_widget_t *widget, int min, int max, int thumb, int page);

bool fbtk_set_scroll_position(fbtk_widget_t *widget, int pos);





/** Create a user widget.
 *
 * Create a widget which is to be handled entirely by the calling application.
 *
 * @param window The window to add the user widget to.
 * @param pw The private pointer which can be read using ::fbtk_get_pw
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_user(fbtk_widget_t *window, int x, int y, int width, int height, void *pw);

void *fbtk_get_userpw(fbtk_widget_t *widget);



/** Create a bitmap widget.
 *
 * Create a widget which shows a bitmap.
 *
 * @param window The window to add the bitmap widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_bitmap(fbtk_widget_t *window, int x, int y, int width, int height, colour c,struct fbtk_bitmap *image);

void fbtk_set_bitmap(fbtk_widget_t *widget, struct fbtk_bitmap *image);

/** Create a button widget.
 *
 * Helper function which creates a bitmap widget and associate a handler for
 * when it is clicked.
 *
 * @param window The window to add the button widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_button(fbtk_widget_t *window, int x, int y, int width, int height, colour c, struct fbtk_bitmap *image, fbtk_callback click, void *pw);





/** Create a text widget.
 *
 * @param window The window to add the text widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_text(fbtk_widget_t *window, int x, int y, int width, int height, colour bg, colour fg, bool outline);

/** Create a button with text.
 *
 * @param window The window to add the text widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_text_button(fbtk_widget_t *window, int x, int y, int width, int height, colour bg, colour fg, fbtk_callback click, void *pw);

/** Create a writable text widget.
 *
 * Helper function which creates a text widget and configures an input handler
 * to create a writable text field. This call is equivalent to calling
 * ::fbtk_create_text followed by ::fbtk_writable_text
 *
 * @param window The window to add the text widget to.
 * @return new widget handle or NULL on error.
 */
fbtk_widget_t *fbtk_create_writable_text(fbtk_widget_t *window, int x, int y, int width, int height, colour bg, colour fg, bool outline, fbtk_enter_t enter, void *pw);

/** Alter a text widget to be writable.
 *
 * @param widget Text widget.
 * @param enter The routine to call when enter is pressed.
 * @param pw The context to pass to teh enter callback routine.
 */
void fbtk_writable_text(fbtk_widget_t *widget, fbtk_enter_t enter, void *pw);

/** Change the text of a text widget.
 *
 * @param widget Text widget.
 * @param text The new UTF-8 text to put in the widget.
 */
void fbtk_set_text(fbtk_widget_t *widget, const char *text);


/** Give widget input focus.
 *
 * @param widget Widget to be given input focus.
 */
void fbtk_set_focus(fbtk_widget_t *widget);




/** enable the on screen keyboard for input */
void fbtk_enable_oskb(fbtk_widget_t *widget);

/** show the osk. */
void map_osk(void);

#endif
