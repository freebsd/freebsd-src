/*
 * Copyright 2013 Stephen Fryatt <stevef@netsurf-browser.org>
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

/** \file mouse.c
 * Mouse dragging and tracking support (implementation).
 *
 * Two different functions are provided:-
 *
 * 1. Wimp_DragBox support, allowing clients to start a drag and specify
 *    callbacks to be used
 *
 *    - on Null Polls while the drag is active,
 *    - when the drag terminates with Event_DragEnd, and
 *    - when the drag terminates with Escape being pressed.
 *
 * 2. Mouse tracking support, allowing clients to track the mouse while it
 *    remains in the current window and specify callbacks to be used
 *
 *    - on Null Polls while the pointer is in the window, and
 *    - when the pointer leaves the window.
 */

#include "oslib/wimp.h"

#include "riscos/mouse.h"
#include "utils/log.h"
#include "utils/utils.h"

/* Data for the wimp drag handler. */

static void (*ro_mouse_drag_end_callback)(wimp_dragged *dragged, void *data)
		= NULL;
static void (*ro_mouse_drag_track_callback)(wimp_pointer *pointer, void *data)
		= NULL;
static void (*ro_mouse_drag_cancel_callback)(void *data) = NULL;
static void *ro_mouse_drag_data = NULL;

static bool ro_mouse_ignore_leaving_event = false;

/* Data for the wimp poll handler. */

static void (*ro_mouse_poll_end_callback)(wimp_leaving *leaving, void *data)
		= NULL;
static void (*ro_mouse_poll_track_callback)(wimp_pointer *pointer, void *data)
		= NULL;
static void *ro_mouse_poll_data = NULL;


/**
 * Process Null polls for any drags and mouse trackers that are currently
 * active.
 */

void ro_mouse_poll(void)
{
	wimp_pointer	pointer;
	os_error	*error;

	/* If no trackers are active, just exit. */

	if (ro_mouse_drag_track_callback == NULL &&
			ro_mouse_poll_track_callback == NULL)
		return;

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* Process the drag tracker, if one is active. */

	if (ro_mouse_drag_track_callback != NULL)
		ro_mouse_drag_track_callback(&pointer, ro_mouse_drag_data);

	/* Process the window tracker, if one is active. */

	if (ro_mouse_poll_track_callback != NULL)
		ro_mouse_poll_track_callback(&pointer, ro_mouse_poll_data);
}


/**
 * Start a drag, providing a function to be called when the Wimp_DragEnd event
 * is received and optionally a tracking function to be called on null polls
 * in between times.
 *
 * \param *drag_end	Callback for when the drag terminates, or NULL for none.
 * \param *drag_track	Callback for mouse tracking during the drag, or NULL for
 *			none.
 * \param *drag_cancel	Callback for cancelling the drag, or NULL if the drag
 *			can't be cancelled.
 * \param *data		Data to be passed to the callback functions, or NULL.
 */
 
void ro_mouse_drag_start(void (*drag_end)(wimp_dragged *dragged, void *data),
		void (*drag_track)(wimp_pointer *pointer, void *data),
		void (*drag_cancel)(void *data), void *data)
{
	/* A drag should never be started when one is already in progress. */

	assert(ro_mouse_drag_end_callback == NULL &&
			ro_mouse_drag_track_callback == NULL &&
			ro_mouse_drag_cancel_callback == NULL &&
			ro_mouse_drag_data == NULL);

	ro_mouse_drag_end_callback = drag_end;
	ro_mouse_drag_track_callback = drag_track;
	ro_mouse_drag_cancel_callback = drag_cancel;
	ro_mouse_drag_data = data;

	/* The Wimp sends a PointerLeaving event when Wimp_DragBox is called,
	 * so we mask out the next event that will come our way.
	 */

	ro_mouse_ignore_leaving_event = true;
}


/**
 * Process Wimp_DragEnd events by terminating an active drag track and passing
 * the details on to any registered event handler.
 *
 * \param *dragged	The Wimp_DragEnd data block.
 */

void ro_mouse_drag_end(wimp_dragged *dragged)
{
	if (ro_mouse_drag_end_callback != NULL)
		ro_mouse_drag_end_callback(dragged, ro_mouse_drag_data);
	else
		warn_user("WimpError", "No callback");

	/* Wimp_DragEnd is a one-shot event, so clear the data ready for
	 * another claimant.
	 */

	ro_mouse_drag_end_callback = NULL;
	ro_mouse_drag_track_callback = NULL;
	ro_mouse_drag_cancel_callback = NULL;
	ro_mouse_drag_data = NULL;
}


/**
 * Start tracking the mouse in a window, providing a function to be called on
 * null polls and optionally one to be called when it leaves the window.
 *
 * \param *poll_end	Callback for when the pointer leaves the window, or
 *			NULL for none. Claimants can receive *leaving==NULL if
 *			a new tracker is started before a PointerLeaving event
 *			is received.
 * \param *poll_track	Callback for mouse tracking while the pointer remains
 *			in the window, or NULL for none.
 * \param *data		Data to be passed to the callback functions, or NULL.
 */

void ro_mouse_track_start(void (*poll_end)(wimp_leaving *leaving, void *data),
		void (*poll_track)(wimp_pointer *pointer, void *data),
		void *data)
{
	/* It should never be possible for the mouse to be in two windows
	 * at the same time! However, some third-party extensions to RISC OS
	 * appear to make this possible (MouseAxess being one), so in the
	 * event that there's still a claimant we tidy them up first and then
	 * log the fact in case there are any unexpected consequences.
	 *
	 * NB: The Poll End callback will get called with *leaving == NULL in
	 * this situation, as there's no PointerLeaving event to pass on.
	 */

	if (ro_mouse_poll_end_callback != NULL ||
			ro_mouse_poll_track_callback != NULL ||
			ro_mouse_poll_data != NULL) {
		if (ro_mouse_poll_end_callback != NULL &&
				ro_mouse_ignore_leaving_event == false)
			ro_mouse_poll_end_callback(NULL, ro_mouse_poll_data);

		LOG(("Unexpected mouse track termination."));

		ro_mouse_ignore_leaving_event = false;
		ro_mouse_poll_end_callback = NULL;
		ro_mouse_poll_track_callback = NULL;
		ro_mouse_poll_data = NULL;
	}

	/* Now record details of the new claimant. */

	ro_mouse_poll_end_callback = poll_end;
	ro_mouse_poll_track_callback = poll_track;
	ro_mouse_poll_data = data;
}


/**
 * Process Wimp_PointerLeaving events by terminating an active mouse track and
 * passing the details on to any registered event handler.
 *
 * If the ignore mask is set, we don't pass the event on to the client as it
 * is assumed that it's a result of starting a Wimp_DragBox operation.
 *
 * \param *leaving	The Wimp_PointerLeaving data block.
 */

void ro_mouse_pointer_leaving_window(wimp_leaving *leaving)
{
	if (ro_mouse_poll_end_callback != NULL &&
			ro_mouse_ignore_leaving_event == false)
		ro_mouse_poll_end_callback(leaving, ro_mouse_poll_data);

	ro_mouse_ignore_leaving_event = false;

	/* Poll tracking is a one-shot event, so clear the data ready for
	 * another claimant.
	 */

	ro_mouse_poll_end_callback = NULL;
	ro_mouse_poll_track_callback = NULL;
	ro_mouse_poll_data = NULL;
}


/**
 * Kill any tracking events if the data pointers match the supplied pointer.
 *
 * \param *data		The data of the client to be killed.
 */

void ro_mouse_kill(void *data)
{
	if (data == ro_mouse_drag_data) {
		ro_mouse_drag_end_callback = NULL;
		ro_mouse_drag_track_callback = NULL;
		ro_mouse_drag_cancel_callback = NULL;
		ro_mouse_drag_data = NULL;
	}

	if (data == ro_mouse_poll_data) {
		ro_mouse_poll_end_callback = NULL;
		ro_mouse_poll_track_callback = NULL;
		ro_mouse_poll_data = NULL;
	}
}


/**
 * Return the desired polling interval to allow the mouse tracking to be
 * carried out.
 *
 * \return		Desired poll interval (0 for none required).
 */

os_t ro_mouse_poll_interval(void)
{
	if (ro_mouse_drag_track_callback != NULL)
		return 4;

	if (ro_mouse_poll_track_callback != NULL)
		return 10;

	return 0;

}

