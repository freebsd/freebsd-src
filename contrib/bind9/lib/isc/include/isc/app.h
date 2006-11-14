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

/* $Id: app.h,v 1.1.206.1 2004/03/06 08:14:38 marka Exp $ */

#ifndef ISC_APP_H
#define ISC_APP_H 1

/*****
 ***** Module Info
 *****/

/*
 * ISC Application Support
 *
 * Dealing with program termination can be difficult, especially in a
 * multithreaded program.  The routines in this module help coordinate
 * the shutdown process.  They are used as follows by the initial (main)
 * thread of the application:
 *
 *		isc_app_start();	Call very early in main(), before
 *					any other threads have been created.
 *
 *		isc_app_run();		This will post any on-run events,
 *					and then block until application
 *					shutdown is requested.  A shutdown
 *					request is made by calling
 *					isc_app_shutdown(), or by sending
 *					SIGINT or SIGTERM to the process.
 *					After isc_app_run() returns, the
 *					application should shutdown itself.
 *
 *		isc_app_finish();	Call very late in main().
 *
 * Applications that want to use SIGHUP/isc_app_reload() to trigger reloading
 * should check the result of isc_app_run() and call the reload routine if
 * the result is ISC_R_RELOAD.  They should then call isc_app_run() again
 * to resume waiting for reload or termination.
 *
 * Use of this module is not required.  In particular, isc_app_start() is
 * NOT an ISC library initialization routine.
 *
 * MP:
 *	Clients must ensure that isc_app_start(), isc_app_run(), and
 *	isc_app_finish() are called at most once.  isc_app_shutdown()
 *	is safe to use by any thread (provided isc_app_start() has been
 *	called previously).
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	None.
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

#include <isc/eventclass.h>
#include <isc/lang.h>
#include <isc/result.h>

typedef isc_event_t isc_appevent_t;

#define ISC_APPEVENT_FIRSTEVENT		(ISC_EVENTCLASS_APP + 0)
#define ISC_APPEVENT_SHUTDOWN		(ISC_EVENTCLASS_APP + 1)
#define ISC_APPEVENT_LASTEVENT		(ISC_EVENTCLASS_APP + 65535)

ISC_LANG_BEGINDECLS

isc_result_t
isc_app_start(void);
/*
 * Start an ISC library application.
 *
 * Notes:
 *	This call should be made before any other ISC library call, and as
 *	close to the beginning of the application as possible.
 */

isc_result_t
isc_app_onrun(isc_mem_t *mctx, isc_task_t *task, isc_taskaction_t action,
	      void *arg);
/*
 * Request delivery of an event when the application is run.
 *
 * Requires:
 *	isc_app_start() has been called.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 */

isc_result_t
isc_app_run(void);
/*
 * Run an ISC library application.
 *
 * Notes:
 *	The caller (typically the initial thread of an application) will
 *	block until shutdown is requested.  When the call returns, the
 *	caller should start shutting down the application.
 *
 * Requires:
 *	isc_app_start() has been called.
 *
 * Ensures:
 *	Any events requested via isc_app_onrun() will have been posted (in
 *	FIFO order) before isc_app_run() blocks.
 *
 * Returns:
 *	ISC_R_SUCCESS			Shutdown has been requested.
 *	ISC_R_RELOAD			Reload has been requested.
 */

isc_result_t
isc_app_shutdown(void);
/*
 * Request application shutdown.
 *
 * Notes:
 *	It is safe to call isc_app_shutdown() multiple times.  Shutdown will
 *	only be triggered once.
 *
 * Requires:
 *	isc_app_run() has been called.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_app_reload(void);
/*
 * Request application reload.
 *
 * Requires:
 *	isc_app_run() has been called.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_UNEXPECTED
 */

void
isc_app_finish(void);
/*
 * Finish an ISC library application.
 *
 * Notes:
 *	This call should be made at or near the end of main().
 *
 * Requires:
 *	isc_app_start() has been called.
 *
 * Ensures:
 *	Any resources allocated by isc_app_start() have been released.
 */

void
isc_app_block(void);
/*
 * Indicate that a blocking operation will be performed.
 *
 * Notes:
 *	If a blocking operation is in process, a call to isc_app_shutdown()
 *	or an external signal will abort the program, rather than allowing
 *	clean shutdown.  This is primarily useful for reading user input.
 *
 * Requires:
 * 	isc_app_start() has been called.
 * 	No other blocking operations are in progress.
 */

void
isc_app_unblock(void);
/*
 * Indicate that a blocking operation is complete.
 *
 * Notes:
 * 	When a blocking operation has completed, return the program to a
 * 	state where a call to isc_app_shutdown() or an external signal will
 * 	shutdown normally.
 *
 * Requires:
 * 	isc_app_start() has been called.
 * 	isc_app_block() has been called by the same thread.
 */


ISC_LANG_ENDDECLS

#endif /* ISC_APP_H */
