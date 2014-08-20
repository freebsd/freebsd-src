/*
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
  * Text selection code (platform-dependent implementation)
  */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "oslib/osfile.h"
#include "oslib/wimp.h"

#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "content/hlcache.h"
#include "desktop/gui.h"
#include "desktop/textinput.h"

#include "riscos/gui.h"
#include "riscos/menus.h"
#include "riscos/message.h"
#include "riscos/mouse.h"
#include "riscos/save.h"
#include "riscos/textselection.h"
#include "riscos/ucstables.h"


#ifndef wimp_DRAG_CLAIM_SUPPRESS_DRAGBOX
#define wimp_DRAG_CLAIM_SUPPRESS_DRAGBOX ((wimp_drag_claim_flags) 0x2u)
#endif


/** Receive of Dragging message has claimed it */
static bool dragging_claimed = false;
static wimp_t dragging_claimant;
static os_box dragging_box = { -34, -34, 34, 34 };  /* \todo - size properly */
static wimp_drag_claim_flags last_claim_flags = 0;
static struct gui_window *last_start_window;

static bool drag_claimed = false;

static bool owns_clipboard = false;
static bool owns_caret_and_selection = false;

/* Current clipboard contents if we own the clipboard 
 * Current paste buffer if we don't
 */
static char *clipboard = NULL;
static size_t clip_length = 0;

/* Paste context */
static ro_gui_selection_prepare_paste_cb paste_cb = NULL;
static void *paste_cb_pw = NULL;
static int paste_prev_message = 0;

static void ro_gui_selection_drag_end(wimp_dragged *drag, void *g);
static void ro_gui_discard_clipboard_contents(void);
static void ro_gui_dragging_bounced(wimp_message *message);


/**
 * Start drag-selecting text within a browser window (RO-dependent part)
 *
 * \param g  gui window
 */

void gui_start_selection(struct gui_window *g)
{
	wimp_full_message_claim_entity msg;
	wimp_auto_scroll_info scroll;
	wimp_window_state state;
	wimp_drag drag;
	os_error *error;

	LOG(("starting text_selection drag"));

	state.w = g->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* claim caret and selection */
	msg.size = sizeof(msg);
	msg.your_ref = 0;
	msg.action = message_CLAIM_ENTITY;
	msg.flags = wimp_CLAIM_CARET_OR_SELECTION;

	error = xwimp_send_message(wimp_USER_MESSAGE,
			(wimp_message*)&msg, wimp_BROADCAST);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	owns_caret_and_selection = true;

	scroll.w = g->window;
	scroll.pause_zone_sizes.x0 = 80;
	scroll.pause_zone_sizes.y0 = 80;
	scroll.pause_zone_sizes.x1 = 80;
	scroll.pause_zone_sizes.y1 = 80;
	scroll.pause_duration = 0;
	scroll.state_change = (void *)0;
	error = xwimp_auto_scroll(wimp_AUTO_SCROLL_ENABLE_VERTICAL |
			wimp_AUTO_SCROLL_ENABLE_HORIZONTAL,
			&scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s",
				error->errnum, error->errmess));

	ro_mouse_drag_start(ro_gui_selection_drag_end, ro_gui_window_mouse_at,
			NULL, g);

	drag.type = wimp_DRAG_USER_POINT;
	/* Don't constrain mouse pointer during drags */
	drag.bbox.x0 = -16384;
	drag.bbox.y0 = -16384;
	drag.bbox.x1 = 16384;
	drag.bbox.y1 = 16384;

	error = xwimp_drag_box(&drag);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	last_start_window = g;
}


/**
 * End of text selection drag operation
 *
 * \param *drag		position of pointer at conclusion of drag
 * \param *data		gui window pointer.
 */

static void ro_gui_selection_drag_end(wimp_dragged *drag, void *data)
{
	wimp_auto_scroll_info scroll;
	wimp_pointer pointer;
	os_error *error;
	os_coord pos;
	struct gui_window *g = (struct gui_window *) data;

	scroll.w = g->window;
	error = xwimp_auto_scroll(0, &scroll, 0);
	if (error)
		LOG(("xwimp_auto_scroll: 0x%x: %s",
				error->errnum, error->errmess));

	error = xwimp_drag_box((wimp_drag*)-1);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info 0x%x : %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (ro_gui_window_to_window_pos(g, drag->final.x0, drag->final.y0, &pos))
		browser_window_mouse_track(g->bw, 0, pos.x, pos.y);
}

/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
static void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles)
{
	char *new_cb;

	if (length == 0)
		return;

	new_cb = malloc(length);
	if (new_cb == NULL)
		return;

	memcpy(new_cb, buffer, length);

	/* Replace existing clipboard contents */
	free(clipboard);
	clipboard = new_cb;
	clip_length = length;

	if (!owns_clipboard) {
		/* Tell RO we now own clipboard */
		wimp_full_message_claim_entity msg;
		os_error *error;

		LOG(("claiming clipboard"));

		msg.size = sizeof(msg);
		msg.your_ref = 0;
		msg.action = message_CLAIM_ENTITY;
		msg.flags = wimp_CLAIM_CLIPBOARD;

		error = xwimp_send_message(wimp_USER_MESSAGE,
				(wimp_message*)&msg, wimp_BROADCAST);
		if (error) {
			LOG(("xwimp_send_message: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		owns_clipboard = true;
	}

	LOG(("clipboard now holds %zd bytes", clip_length));
}


/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yielded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
static void gui_get_clipboard(char **buffer, size_t *length)
{
	*buffer = NULL;
	*length = 0;

	if (clip_length > 0) {
		char *cb = malloc(clip_length);
		if (cb != NULL) {
			memcpy(cb, clipboard, clip_length);
			*buffer = cb;
			*length = clip_length;
		}
	}
}


/**
 * Discard the current contents of the clipboard, if any, releasing the
 * memory it uses.
 */

void ro_gui_discard_clipboard_contents(void)
{
	free(clipboard);
	clipboard = NULL;
	clip_length = 0;
}


static void ro_gui_selection_prepare_paste_complete(void)
{
	ro_gui_selection_prepare_paste_cb cb = paste_cb;
	void *pw = paste_cb_pw;

	paste_cb = NULL;
	paste_cb_pw = NULL;
	paste_prev_message = 0;

	cb(pw);
}

static void ro_gui_selection_prepare_paste_bounced(wimp_message *message)
{
	ro_gui_selection_prepare_paste_complete();
}

/**
 * Prepare to paste data from another application
 *
 * \param w   Window being pasted into
 * \param cb  Callback to call once preparation is complete
 * \param pw  Private data for callback
 */

void ro_gui_selection_prepare_paste(wimp_w w,
		ro_gui_selection_prepare_paste_cb cb, void *pw)
{
	if (owns_clipboard) {
		/* We own the clipboard: we're already prepared */
		cb(pw);
	} else {
		/* Someone else owns the clipboard: request its contents */
		wimp_full_message_data_request msg;
		bool success;

		ro_gui_discard_clipboard_contents();

		msg.size = 48; /* There's only one filetype listed. */
		msg.your_ref = 0;
		msg.action = message_DATA_REQUEST;
		msg.w = w;
		msg.i = -1;
		msg.pos.x = 0;
		msg.pos.y = 0;
		msg.flags = wimp_DATA_REQUEST_CLIPBOARD;
		msg.file_types[0] = osfile_TYPE_TEXT;
		msg.file_types[1] = ~0;

		success = ro_message_send_message(wimp_USER_MESSAGE_RECORDED,
				(wimp_message *) &msg, wimp_BROADCAST,
				ro_gui_selection_prepare_paste_bounced);
		if (success == false) {
			/* Ensure key is handled, anyway */
			cb(pw);
		} else {
			/* Set up paste context */
			paste_cb = cb;
			paste_cb_pw = pw;
			paste_prev_message = msg.my_ref;
		}
	}
}

/**
 * Prepare to paste data from another application (step 2)
 *
 * \param dataxfer  DataSave message
 * \return True if message was handled, false otherwise
 */
bool ro_gui_selection_prepare_paste_datasave(
		wimp_full_message_data_xfer *dataxfer)
{
	bool success;

	/* Ignore messages that aren't for us */
	if (dataxfer->your_ref == 0 || dataxfer->your_ref != paste_prev_message)
		return false;

	/* We're done if the paste data isn't text */
	if (dataxfer->file_type != osfile_TYPE_TEXT) {
		ro_gui_selection_prepare_paste_complete();
		return true;
	}

	/* Generate and send DataSaveAck */
	dataxfer->your_ref = dataxfer->my_ref;
	dataxfer->size = offsetof(wimp_full_message_data_xfer, file_name) + 16;
	dataxfer->action = message_DATA_SAVE_ACK;
	dataxfer->est_size = -1;
	memcpy(dataxfer->file_name, "<Wimp$Scrap>", SLEN("<Wimp$Scrap>") + 1);

	success = ro_message_send_message(wimp_USER_MESSAGE_RECORDED,
			(wimp_message *) dataxfer, dataxfer->sender,
			ro_gui_selection_prepare_paste_bounced);
	if (success == false) {
		ro_gui_selection_prepare_paste_complete();
	} else {
		paste_prev_message = dataxfer->my_ref;
	}

	return true;
}


/**
 * Prepare to paste data from another application (step 3)
 *
 * \param dataxfer  DataLoad message
 * \return True if message was handled, false otherwise
 */
bool ro_gui_selection_prepare_paste_dataload(
		wimp_full_message_data_xfer *dataxfer)
{
	FILE *fp;
	long size;
	char *local_cb;
	nserror ret;

	/* Ignore messages that aren't for us */
	if (dataxfer->your_ref == 0 || dataxfer->your_ref != paste_prev_message)
		return false;

	fp = fopen(dataxfer->file_name, "r");
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		if (size > 0) {
			local_cb = malloc(size);
			if (local_cb != NULL) {
				fread(local_cb, 1, size, fp);

				ret = utf8_from_local_encoding(local_cb, size,
						&clipboard);
				if (ret == NSERROR_OK) {
					clip_length = strlen(clipboard);
				}

				free(local_cb);
			}
		}

		fclose(fp);
	}

	/* Send DataLoadAck */
	dataxfer->action = message_DATA_LOAD_ACK;
	dataxfer->your_ref = dataxfer->my_ref;
	ro_message_send_message(wimp_USER_MESSAGE,
			(wimp_message *) dataxfer, dataxfer->sender, NULL);

	ro_gui_selection_prepare_paste_complete();
	return true;
}


/**
 * Responds to CLAIM_ENTITY message notifying us that the caret
 * and selection or clipboard have been claimed by another application.
 *
 * \param claim  CLAIM_ENTITY message
 */

void ro_gui_selection_claim_entity(wimp_full_message_claim_entity *claim)
{
	/* ignore our own broadcasts! */
	if (claim->sender != task_handle) {

		LOG(("%x", claim->flags));

		if (claim->flags & wimp_CLAIM_CARET_OR_SELECTION) {
			owns_caret_and_selection = false;
		}

		if (claim->flags & wimp_CLAIM_CLIPBOARD) {
			ro_gui_discard_clipboard_contents();
			owns_clipboard = false;
		}
	}
}


/**
 * Responds to DATA_REQUEST message, returning information about the
 * clipboard contents if we own the clipboard.
 *
 * \param  req  DATA_REQUEST message
 */

void ro_gui_selection_data_request(wimp_full_message_data_request *req)
{
	if (owns_clipboard && clip_length > 0 &&
		(req->flags & wimp_DATA_REQUEST_CLIPBOARD)) {
		wimp_full_message_data_xfer message;
		int size;
//		int i;

//		for(i = 0; i < NOF_ELEMENTS(req->file_types); i++) {
//			bits ftype = req->file_types[i];
//			if (ftype == ~0U) break;	/* list terminator */
//
//			LOG(("type %x", ftype));
//			i++;
//		}

		/* we can only supply text at the moment, so that's what you're getting! */
		size = offsetof(wimp_full_message_data_xfer, file_name) + 9;
		message.size = (size + 3) & ~3;
		message.your_ref = req->my_ref;
		message.action = message_DATA_SAVE;
		message.w = req->w;
		message.i = req->i;
		message.pos = req->pos;
		message.file_type = osfile_TYPE_TEXT;
		message.est_size = clip_length;
		memcpy(message.file_name, "TextFile", 9);

		ro_gui_send_datasave(GUI_SAVE_CLIPBOARD_CONTENTS,
				&message, req->sender);
	}
}


/**
 * Save the clipboard contents to a file.
 *
 * \param  path  the pathname of the file
 * \return true iff success, otherwise reporting the error before returning false
 */

bool ro_gui_save_clipboard(const char *path)
{
	char *local_cb;
	nserror ret;
	os_error *error;

	assert(clip_length > 0 && clipboard);

	ret = utf8_to_local_encoding(clipboard, clip_length, &local_cb);
	if (ret != NSERROR_OK) {
		warn_user("SaveError", "Could not convert");
		return false;
	}

	error = xosfile_save_stamped(path, osfile_TYPE_TEXT,
			(byte*) local_cb,
			(byte*) local_cb + strlen(local_cb));

	free(local_cb);

	if (error) {
		LOG(("xosfile_save_stamped: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("SaveError", error->errmess);
		return false;
	}

	return true;
}


/**
 * Handler for Message_Dragging, used to implement auto-scrolling and
 * ghost caret when a drag is in progress.
 */

void ro_gui_selection_dragging(wimp_message *message)
{
	wimp_full_message_dragging *drag = (wimp_full_message_dragging*)message;
	struct gui_window *g;
	os_coord pos;

	/* with autoscrolling, we will probably need to remember the
	 * gui_window and override the drag->w window handle which
	 * could be any window on the desktop */
	g = ro_gui_window_lookup(drag->w);

	if ((drag->flags & wimp_DRAGGING_TERMINATE_DRAG) || !g) {

		drag_claimed = false;
		return;
	}

	if (!ro_gui_window_to_window_pos(g, drag->pos.x, drag->pos.y, &pos))
		return;

	drag_claimed = false;
}



/**
 * Reset drag-and-drop state when drag completes (DataSave received)
 */

void ro_gui_selection_drag_reset(void)
{
	drag_claimed = false;
}


/**
 *
 */

void ro_gui_selection_drag_claim(wimp_message *message)
{
	wimp_full_message_drag_claim *claim = (wimp_full_message_drag_claim*)message;

	dragging_claimant = message->sender;
	dragging_claimed = true;

	/* have we been asked to remove the drag box/sprite? */
	if (claim->flags & wimp_DRAG_CLAIM_SUPPRESS_DRAGBOX) {
		ro_gui_drag_box_cancel();
	}
	else {
		/* \todo - restore it here? */
	}

	/* do we need to restore the default pointer shape? */
	if ((last_claim_flags & wimp_DRAG_CLAIM_POINTER_CHANGED) &&
			!(claim->flags & wimp_DRAG_CLAIM_POINTER_CHANGED)) {
		gui_window_set_pointer(last_start_window, GUI_POINTER_DEFAULT);
	}

	last_claim_flags = claim->flags;
}


void ro_gui_selection_send_dragging(wimp_pointer *pointer)
{
	wimp_full_message_dragging dragmsg;

	LOG(("sending DRAGGING to %p, %d", pointer->w, pointer->i));

	dragmsg.size = offsetof(wimp_full_message_dragging, file_types) + 8;
	dragmsg.your_ref = 0;
	dragmsg.action = message_DRAGGING;
	dragmsg.w = pointer->w;
	dragmsg.i = pointer->i;
	dragmsg.pos = pointer->pos;
/* \todo - this is interesting because it depends upon not just the state of the
      shift key, but also whether it /can/ be deleted, ie. from text area/input
      rather than page contents */
	dragmsg.flags = wimp_DRAGGING_FROM_SELECTION;
	dragmsg.box = dragging_box;
	dragmsg.file_types[0] = osfile_TYPE_TEXT;
	dragmsg.file_types[1] = ~0;

	/* if the message_dragmsg messages have been claimed we must address them
	   to the claimant task, which is not necessarily the task that owns whatever
	   window happens to be under the pointer */

	if (dragging_claimed) {
		ro_message_send_message(wimp_USER_MESSAGE_RECORDED,
				(wimp_message*)&dragmsg, dragging_claimant, ro_gui_dragging_bounced);
	}
	else {
		ro_message_send_message_to_window(wimp_USER_MESSAGE_RECORDED,
				(wimp_message*)&dragmsg, pointer->w, pointer->i,
				ro_gui_dragging_bounced, &dragging_claimant);
	}
}


/**
 * Our message_DRAGGING message was bounced, ie. the intended recipient does not
 * support the drag-and-drop protocol or cannot receive the data at the pointer
 * position.
 */

void ro_gui_dragging_bounced(wimp_message *message)
{
	dragging_claimed = false;
}

static struct gui_clipboard_table clipboard_table = {
	.get = gui_get_clipboard,
	.set = gui_set_clipboard,
};

struct gui_clipboard_table *riscos_clipboard_table = &clipboard_table;
