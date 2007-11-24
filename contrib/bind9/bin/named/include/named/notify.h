/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: notify.h,v 1.9.208.1 2004/03/06 10:21:25 marka Exp $ */

#ifndef NAMED_NOTIFY_H
#define NAMED_NOTIFY_H 1

#include <named/types.h>
#include <named/client.h>

/***
 ***	Module Info
 ***/

/*
 *	RFC 1996
 *	A Mechanism for Prompt Notification of Zone Changes (DNS NOTIFY)
 */

/***
 ***	Functions.
 ***/

void
ns_notify_start(ns_client_t *client);

/*
 *	Examines the incoming message to determine apporiate zone.
 *	Returns FORMERR if there is not exactly one question.
 *	Returns REFUSED if we do not serve the listed zone.
 *	Pass the message to the zone module for processing
 *	and returns the return status.
 *
 * Requires
 *	client to be valid.
 */

#endif /* NAMED_NOTIFY_H */

