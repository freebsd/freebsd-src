/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Automated RISC OS message routing (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "riscos/message.h"
#include "utils/log.h"
#include "utils/utils.h"


struct active_message {
	unsigned int message_code;
	int id;
	void (*callback)(wimp_message *message);
	struct active_message *next;
	struct active_message *previous;
};
struct active_message *current_messages = NULL;

static struct active_message *ro_message_add(unsigned int message_code,
		void (*callback)(wimp_message *message));
static void ro_message_free(int ref);


/**
 * Sends a message and registers a return route for a bounce.
 *
 * \param event		the message event type
 * \param message	the message to register a route back for
 * \param task		the task to send a message to, or 0 for broadcast
 * \param callback	the code to call on a bounce
 * \return true on success, false otherwise
 */
bool ro_message_send_message(wimp_event_no event, wimp_message *message,
		wimp_t task, void (*callback)(wimp_message *message))
{
	os_error *error;

	assert(message);

	/* send a message */
	error = xwimp_send_message(event, message, task);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* register the default bounce handler */
	if (callback) {
		assert(event == wimp_USER_MESSAGE_RECORDED);
		return ro_message_register_handler(message, message->action,
				callback);
	}
	return true;
}


/**
 * Sends a message and registers a return route for a bounce.
 *
 * \param event		the message event type
 * \param message	the message to register a route back for
 * \param to_w		the window to send the message to
 * \param to_i		the icon
 * \param callback	the code to call on a bounce
 * \param to_t		receives the task handle of the window's creator
 * \return true on success, false otherwise
 */
bool ro_message_send_message_to_window(wimp_event_no event, wimp_message *message,
		wimp_w to_w, wimp_i to_i, void (*callback)(wimp_message *message),
		wimp_t *to_t)
{
	os_error *error;

	assert(message);

	/* send a message */
	error = xwimp_send_message_to_window(event, message, to_w, to_i, to_t);
	if (error) {
		LOG(("xwimp_send_message_to_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* register the default bounce handler */
	if (callback) {
		assert(event == wimp_USER_MESSAGE_RECORDED);
		return ro_message_register_handler(message, message->action,
				callback);
	}
	return true;
}


/**
 * Registers a return route for a message.
 *
 * This function must be called after wimp_send_message so that a
 * valid value is present in the my_ref field.
 *
 * \param message	the message to register a route back for
 * \param message_code	the message action code to route
 * \param callback	the code to call for a matched action
 * \return true on success, false on memory exhaustion
 */
bool ro_message_register_handler(wimp_message *message,
		unsigned int message_code,
		void (*callback)(wimp_message *message))
{
	struct active_message *add;

	assert(message);
	assert(callback);

	add = ro_message_add(message_code, callback);
	if (add)
		add->id = message->my_ref;
	return (add != NULL);
}


/**
 * Registers a route for a message code.
 *
 * \param message_code	the message action code to route
 * \param callback	the code to call for a matched action
 * \return true on success, false on memory exhaustion
 */
bool ro_message_register_route(unsigned int message_code,
		void (*callback)(wimp_message *message))
{
	assert(callback);

	return (ro_message_add(message_code, callback) != NULL);
}

struct active_message *ro_message_add(unsigned int message_code,
		void (*callback)(wimp_message *message))
{
	struct active_message *add;

	assert(callback);

	add = (struct active_message *)malloc(sizeof(*add));
	if (!add)
		return NULL;
	add->message_code = message_code;
	add->id = 0;
	add->callback = callback;
	add->next = current_messages;
	add->previous = NULL;
	current_messages = add;
	return add;
}


/**
 * Attempts to route a message.
 *
 * \param message	the message to attempt to route
 * \return true if message was routed, false otherwise
 */
bool ro_message_handle_message(wimp_event_no event, wimp_message *message)
{
	struct active_message *test;
	bool handled = false;
	int ref;

	assert(message);

	if (event == wimp_USER_MESSAGE_ACKNOWLEDGE) {
		/* handle message acknowledgement */
		ref = message->my_ref;
		if (ref == 0)
			return false;

		/* handle the message */
		for (test = current_messages; test; test = test->next) {
			if ((ref == test->id) &&
					(message->action == test->message_code)) {
				handled = true;
				if (test->callback)
					test->callback(message);
				break;
			}
		}

		/* remove all handlers for this id */
		ro_message_free(ref);
		return handled;
	} else {
		/* handle simple routing */
		for (test = current_messages; test; test = test->next) {
			if ((test->id == 0) &&
					(message->action == test->message_code)) {
				test->callback(message);
				return true;
			}
		}
	}
	return false;
}


void ro_message_free(int ref)
{
	struct active_message *test;
	struct active_message *next = current_messages;

	while ((test = next)) {
		next = test->next;
		if (ref == test->id) {
			if (test->previous)
				test->previous->next = test->next;
			if (test->next)
				test->next->previous = test->previous;
			if (current_messages == test)
				current_messages = test->next;
			free(test);
		}
	}
}
