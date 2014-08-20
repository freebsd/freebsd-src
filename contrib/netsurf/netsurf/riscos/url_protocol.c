/*
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2003 Rob Jackson <jacko@xms.ms>
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * ANT URL launching protocol (implementation).
 *
 * See http://www.vigay.com/inet/inet_url.html
 */

#include "utils/config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "oslib/inetsuite.h"
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

/**
 * Handle a Message_InetSuiteOpenURL.
 */

void ro_url_message_received(wimp_message *message)
{
	char *url;
	int i;
	inetsuite_message_open_url *url_message =
			(inetsuite_message_open_url*) &message->data;
	os_error *error;
	nsurl *nsurl;
	nserror errorns;

	/* If the url_message->indirect.tag is non-zero,
	 * then the message data is contained within the message block.
	 */
	if (url_message->indirect.tag != 0) {
		url = strndup(url_message->url, 236);
		if (!url) {
			warn_user("NoMemory", 0);
			return;
		}
		/* terminate at first control character */
		for (i = 0; !iscntrl(url[i]); i++)
			;
		url[i] = 0;

	} else {
		if (!url_message->indirect.url.offset) {
			LOG(("no URL in message"));
			return;
		}
		if (28 < message->size &&
				url_message->indirect.body_file.offset) {
			LOG(("POST for URL message not implemented"));
			return;
		}
		if (url_message->indirect.url.offset < 28 ||
				236 <= url_message->indirect.url.offset) {
			LOG(("external pointers in URL message unimplemented"));
			/* these messages have never been seen in the wild,
			 * and there is the problem of invalid addresses which
			 * would cause an abort */
			return;
		}

		url = strndup((char *) url_message +
				url_message->indirect.url.offset,
				236 - url_message->indirect.url.offset);
		if (!url) {
			warn_user("NoMemory", 0);
			return;
		}
		for (i = 0; !iscntrl(url[i]); i++)
			;
		url[i] = 0;
	}

	if (nsurl_create(url, &nsurl) != NSERROR_OK) {
		free(url);
		return;
	}

	if (!fetch_can_fetch(nsurl)) {
		nsurl_unref(nsurl);
		free(url);
		return;
	}

	free(url);

	/* send ack */
	message->your_ref = message->my_ref;
	error = xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE, message,
			message->sender);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	/* create new browser window */
	errorns = browser_window_create(BW_CREATE_HISTORY,
				      nsurl,
				      NULL,
				      NULL,
				      NULL);


	nsurl_unref(nsurl);
	if (errorns != NSERROR_OK) {
		warn_user(messages_get_errorcode(errorns), 0);
	}
}


/**
 * Broadcast an ANT URL message.
 */

void ro_url_broadcast(const char *url)
{
	inetsuite_full_message_open_url_direct message;
	os_error *error;
	int len = strlen(url) + 1;

	/* If URL is too long, then forget ANT and try URI, instead */
	if (236 < len) {
		ro_uri_launch(url);
		return;
	}

	message.size = ((20+len+3) & ~3);
	message.your_ref = 0;
	message.action = message_INET_SUITE_OPEN_URL;
	strncpy(message.url, url, 235);
	message.url[235] = 0;
	error = xwimp_send_message(wimp_USER_MESSAGE_RECORDED,
			(wimp_message *) &message, 0);
	if (error) {
		LOG(("xwimp_send_message: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Launch a program to handle an URL, using the ANT protocol
 * Alias$URLOpen_ system.
 */

void ro_url_load(const char *url)
{
	char *command;
	char *colon;
	os_error *error;

	colon = strchr(url, ':');
	if (!colon) {
		LOG(("invalid url '%s'", url));
		return;
	}

	command = malloc(40 + (colon - url) + strlen(url));
	if (!command) {
		warn_user("NoMemory", 0);
		return;
	}

	sprintf(command, "Alias$URLOpen_%.*s", (int) (colon - url), url);
	if (!getenv(command)) {
		free(command);
		return;
	}

	sprintf(command, "URLOpen_%.*s %s", (int) (colon - url), url, url);

	error = xwimp_start_task(command, 0);
	if (error) {
		LOG(("xwimp_start_task: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	free(command);
}


/**
 * Handle a bounced Message_InetSuiteOpenURL.
 */

void ro_url_bounce(wimp_message *message)
{
	inetsuite_message_open_url *url_message =
			(inetsuite_message_open_url*) &message->data;

	/* ant broadcast bounced -> try uri broadcast / load */
	ro_uri_launch(url_message->url);
}

