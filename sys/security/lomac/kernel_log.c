/*-
 * Copyright (c) 2001 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by NAI Labs, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $Id$
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include "lomacfs.h"

#include "kernel_interface.h"

/* The following definition sets the global log verbosity level. */
SYSCTL_NODE(_kern, OID_AUTO, lomac, CTLFLAG_RW, 0, "LOMAC");
SYSCTL_NODE(_kern_lomac, OID_AUTO, verbose, CTLFLAG_RW, 0, "LOMAC verbosity");
#define	VERBOSITY_SETTING(level) 					\
	unsigned int lomac_verbose_##level## = 1;			\
	SYSCTL_UINT(_kern_lomac_verbose, OID_AUTO, level,		\
	    CTLFLAG_RW, &lomac_verbose_##level##, 1, "")
#include "kernel_log.h"

/* sbuf_start()
 *
 * in:     nothing
 * out:    nothing
 * return: struct sbuf * to pass to later callers
 *
 */

lomac_log_t *
log_start(void) {
	struct sbuf *s;

	s = sbuf_new(NULL, NULL, PATH_MAX * 2, 0);
	KASSERT(s != NULL, ("sbuf uses M_WAITOK -- must not return NULL!"));
	return (s);
} /* log_start() */


/* log_append_string()
 *
 * in:     s - a struct sbuf *
 * in:     data_s - null-terminated string to append to log
 * out:    nothing, see description for side-effects
 * return: nothing
 *
 *     This function appends `data_s' to `log_s', being careful to ensure
 * that there is sufficient room in `log_s' for the data and a null 
 * terminator.  If there is insufficient room in `log_s' for the entire
 * `data_s' string, this function will append only the prefix of `data_s'
 * which fits.
 *
 */

void	
log_append_string(lomac_log_t *s, const char *data_s) {

	(void)sbuf_cat(s, data_s);
} /* log_append_string */


/* log_append_int()
 *
 * in:     data - integer value to append to log
 * out:    nothing, see description for side-effects
 * return: nothing
 *
 *     This function determines the ASCII representation of the integer
 * value in `data' and, if there is sufficient room, appends this
 * ASCII representation to `log_s'.  If there is insufficient room,
 * this function behaves as log_append_string().
 *
 */

void
log_append_int(lomac_log_t *s, int data) {

	(void)sbuf_printf(s, "%d", data);
} /* log_append_int() */


/* log_append_subject_id()
 *
 * in:     p_subject - subject whose ID we want to append to the log message
 * out:    nothing, see description for side-effects
 * return: nothing
 *
 *    This function appends a string describing the identity of `p_subject'
 * to `log_s'.  If there is insufficient room in `log_s' for the entire
 * ID string, only a (possibly empty) prefix of the ID string will be
 * appended.
 *
 */

void
log_append_subject_id(lomac_log_t *s, const lomac_subject_t *p_subject) {

	(void)sbuf_printf(s, "p%dg%du%d:%s", p_subject->p_pid,
	    p_subject->p_pgrp->pg_id, p_subject->p_ucred->cr_uid,
	    p_subject->p_comm);
} /* log_append_subject_id() */

/* log_append_object_id()
 *
 * in:     p_object - object whose ID we want to append to the log message
 * out:    nothing, see description for side-effects
 * return: nothing
 *
 *    This function appends a string describing the identity of `p_object'
 * to `log_s'.  If there is insufficient room in `log_s' for the entire
 * ID string, only a (possibly empty) prefix of the ID string will be
 * appended.
 *
 */

void
log_append_object_id(lomac_log_t *s, const lomac_object_t *p_object) {
	struct lomac_node *ln;

	switch (p_object->lo_type) {
	case LO_TYPE_UVNODE:
		(void)sbuf_printf(s, "vp %p", p_object->lo_object.vnode);
		break;
	case LO_TYPE_LVNODE:
		ln = VTOLOMAC(p_object->lo_object.vnode);
#ifdef LOMAC_DEBUG_INCNAME
		(void)sbuf_printf(s, "named \"%s\"", ln->ln_name);
#else
		if (ln->ln_entry != NULL)
			(void)sbuf_printf(s, "named \"%s\"",
			    ln->ln_entry->ln_path);
		else
			(void)sbuf_printf(s, "under \"%s\"",
			    ln->ln_underpolicy->ln_path);
#endif
		break;
	case LO_TYPE_PIPE:
		(void)sbuf_printf(s, "pipe %p", p_object->lo_object.pipe);
		break;
	case LO_TYPE_SOCKETPAIR:
		(void)sbuf_printf(s, "socket %p", p_object->lo_object.socket);
		break;
	default:
		panic("invalid LOMAC object type");
	}
} /* log_append_object_id() */

/* log_print()
 *
 * in:     nothing
 * out:    nothing
 * return: nothing
 *
 *     This function prints `log_s' to the system log.
 *
 */

void
log_print(lomac_log_t *s) {

	sbuf_finish(s);
	log(LOG_INFO, "%s", sbuf_data(s));
	sbuf_delete(s);
} /* log_print() */
