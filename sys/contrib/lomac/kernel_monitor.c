/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * This file contains functions which update LOMAC's internal
 * state in response to system events, such as successful
 * system calls.  These updates allow LOMAC to keep an accurate
 * picture of the kernel's state, enabling LOMAC to make reasonable
 * decisions when it mediates processes' use of security-relevant
 * system calls.  These functions perform no mediation themselves -
 * that is, they do not make access control decisions concerning
 * whether a given system call should be allowed or denied. This
 * mediation is handled by the functions in lomac_mediate.c.
 */

#include "kernel_interface.h"
#include "kernel_monitor.h"
#include "kernel_log.h"

#include "kernel_util.h"


/* monitor_read_object()
 *
 * in:     p_subject - subject that read `p_object'.
 *         p_object  - object read by `p_subject'.
 * out:    nothing
 * return: nothing
 *
 * This function examines the objects read by subjects.  If a subject
 * reads from an object with a level lower than its own, this function
 * reduces the subject's level to match the object's.  This lowering
 * is referred to as "demotion" in much of the LOMAC documentation.
 * This function performs no mediation.
 *
 * This function is for the following kinds of objects:
 *   regular files and FIFOs.
 * It is also used to monitor reads on unnamed pipes.  LOMAC does not
 * consider unnamed pipes to be objects, and treats them differently
 * from objects such as files.  However, the differences are mostly
 * in the write-handling behavior.  LOMAC's read-handling behavior is
 * the same both for objects and unnamed pipes, so this function
 * handles both cases.
 */

int
monitor_read_object(lomac_subject_t *p_subject, lomac_object_t *p_object) {
	lattr_t subject_lattr;     /* lattr of `p_subject' */
	lattr_t object_lattr;      /* lattr of `p_object' */

	/* Get the lattrs of `p_subject' and `p_object' so we can compare them. */
	get_subject_lattr(p_subject, &subject_lattr);
	get_object_lattr(p_object,  &object_lattr);

	/*
	* If `p_object's level is less than `p_subject's level,
	* we must demote `p_subject'.  The level may be 0 to indicate
	* existence before LOMAC started.
	*/
	if (object_lattr.level && 
	    lomac_must_demote(&subject_lattr, &object_lattr) &&
	    (subject_lattr.flags & LOMAC_ATTR_NODEMOTE) == 0) {
		if (subject_do_not_demote(p_subject))
			return (0);
		set_subject_lattr(p_subject, object_lattr);  /* demote! */
		if (lomac_verbose_demote_deny) {
			lomac_log_t *s = log_start();

			log_append_string(s, "LOMAC: level-");
			log_append_int(s, subject_lattr.level);
			log_append_string(s, " subject ");
			log_append_subject_id(s, p_subject);
			log_append_string(s, " demoted to level ");
			log_append_int(s, object_lattr.level);
			log_append_string(s, " after reading ");
			log_append_object_id(s, p_object);
			log_append_string(s, "\n");
			log_print(s);
		}
	} /* if we need to demote */
	return (0);
} /* monitor_read_object() */


/* monitor_pipe_write()
 *
 * in:     p_subject - subject that just wrote to `p_pipe'.
 *         p_pipe - pipe `p_subject' has written to.
 * out:    p_pipe - pipe may have its level adjusted.
 * return: 0
 *
 *     This function should be called after a successful write to
 * `p_pipe'.  If the level of `p_subject' is less than the level of
 * `p_pipe', this function reduces `p_pipe's level to match
 * `current's.
 *
 *
 */

int 
monitor_pipe_write(lomac_subject_t *p_subject, lomac_object_t *p_pipe) {
	lattr_t pipe_lattr;          /* lattr of `p_pipe' */
	lattr_t subject_lattr;       /* lattr of `p_subject' */

	get_subject_lattr(p_subject, &subject_lattr);
	get_object_lattr(p_pipe, &pipe_lattr);
	if (lomac_must_demote(&pipe_lattr, &subject_lattr)) {
		subject_lattr.flags = 0;
		set_object_lattr(p_pipe, subject_lattr);
#ifdef LOMAC_DEBUG_PIPE
		if (lomac_verbose_pipe) {
			lomac_log_t *s = log_start();

			log_append_string(s, "LOMAC: level-");
			log_append_int(s, subject_lattr.level);
			log_append_string(s, " subject ");
			log_append_subject_id(s, p_subject);
			log_append_string(s, " contaminated level-");
			log_append_int(s, pipe_lattr.level);
			log_append_string(s, " ");
			log_append_object_id(s, p_pipe);
			log_append_string(s, "\n");
			log_print(s);
		}
#endif /* LOMAC_DEBUG_PIPE */
	}
	return (0);
} /* monitor_pipe_write() */


/* monitor_read_net_socket()
 *
 * in:     p_subject - subject that read from network socket.
 * out:    nothing
 * return: 0
 *
 *
 */

int
monitor_read_net_socket(lomac_subject_t *p_subject) {
	lattr_t subject_lattr;          /* lattr of `p_subject' */
	lattr_t socket_lattr;           /* lattr of socket (always lowest) */

	socket_lattr.level = LOMAC_LOWEST_LEVEL;
	socket_lattr.flags = 0;
	get_subject_lattr(p_subject, &subject_lattr);

	if (lomac_must_demote(&subject_lattr, &socket_lattr) &&
	    (subject_lattr.flags &
	    (LOMAC_ATTR_NODEMOTE | LOMAC_ATTR_NONETDEMOTE)) == 0) {

		if (subject_do_not_demote(p_subject))
			return (0);
		socket_lattr.flags = subject_lattr.flags;
		set_subject_lattr(p_subject, socket_lattr);  /* demote! */
		if (lomac_verbose_demote_deny) {
			lomac_log_t *s = log_start();

			log_append_string(s, "LOMAC: level-");
			log_append_int(s, subject_lattr.level);
			log_append_string(s, " subject ");
			log_append_subject_id(s, p_subject);
			log_append_string(s, " demoted to level ");
			log_append_int(s, socket_lattr.level);
			log_append_string(s, " after reading from "
			    "the network\n");
			log_print(s);
		}
	}
	return (0);
}
