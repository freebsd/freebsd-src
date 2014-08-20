/*
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
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

#include "utils/config.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/uri.h"
#include "oslib/wimp.h"
#include "utils/config.h"
#include "content/fetch.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "riscos/gui.h"
#include "riscos/uri.h"
#include "riscos/url_protocol.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "utils/utils.h"

void ro_uri_message_received(wimp_message *msg)
{
	uri_full_message_process *uri_message = (uri_full_message_process *)msg;
	uri_h uri_handle;
	char* uri_requested;
	int uri_length;
	nsurl *url;
	nserror error;

	uri_handle = uri_message->handle;

	if (nsurl_create(uri_message->uri, &url) != NSERROR_OK) {
		return;
	}

	if (!fetch_can_fetch(url)) {
		nsurl_unref(url);
		return;
	}

	nsurl_unref(url);

	uri_message->your_ref = uri_message->my_ref;
	uri_message->action = message_URI_PROCESS_ACK;

	xwimp_send_message(wimp_USER_MESSAGE, (wimp_message*)uri_message,
		uri_message->sender);

	xuri_request_uri(0, 0, 0, uri_handle, &uri_length);
	uri_requested = calloc((unsigned int)uri_length, sizeof(char));
	if (uri_requested == NULL)
		return;

	xuri_request_uri(0, uri_requested, uri_length, uri_handle, NULL);

	error = nsurl_create(uri_requested, &url);
	free(uri_requested);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
}

bool ro_uri_launch(const char *uri)
{
	uri_h uri_handle;
	wimp_t handle_task;
	uri_dispatch_flags returned;
	os_error *e;

	e = xuri_dispatch(uri_DISPATCH_INFORM_CALLER, uri, task_handle,
			&returned, &handle_task, &uri_handle);

	if (e || returned & 1) {
		return false;
	}

	return true;
}

void ro_uri_bounce(wimp_message *msg)
{
	uri_full_message_process *message = (uri_full_message_process *)msg;
	int size;
	char *uri_buf;
	os_error *e;

	if ((message->flags & 1) == 0)
		return;

	/* Get required buffer size */
	e = xuri_request_uri(0, NULL, 0, message->handle, &size);
	if (e) {
		LOG(("xuri_request_uri: %d: %s", e->errnum, e->errmess));
		return;
	}

	uri_buf = malloc(size);
	if (uri_buf == NULL)
		return;

	/* Get URI */
	e = xuri_request_uri(0, uri_buf, size, message->handle, 0);
	if (e) {
		LOG(("xuri_request_uri: %d: %s", e->errnum, e->errmess));
		free(uri_buf);
		return;
	}

	ro_url_load(uri_buf);

	free(uri_buf);

	return;
}
