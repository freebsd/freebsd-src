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
 * $Id: kernel_mediate.c,v 1.9 2001/10/17 15:19:40 bfeldman Exp $
 */

/*
 * This file contains functions that make access control decisions
 * concerning wether or not given system calls should be allowed
 * or denied.  This activity is called "mediation".  These functions
 * generally consider both the parameters passed to a system call
 * and the current internal state of LOMAC in the course of making
 * a decision.  However, they do not modify these parameters or
 * LOMAC's internal state.  Functions for modifying LOMAC's internal
 * state can be found in lomac_monitor.c.
 *
 */

#include "kernel_interface.h"
#include "kernel_mediate.h"
#if 0
#include "lomac_plm.h"
#endif
#include "kernel_log.h"

/* mediate_subject_level_subject()
 *
 * in:     op_s          - name of operation to mediate
 *         p_subject_one - subject one (for informational purposes only)
 *         level_one     - already-known level of the first subject
 *         p_subject_two - subject two
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     caller should deny operation
 *           1     caller should allow operation
 *
 *     This function returns 1 if `p_subject_one's level is at least
 * as great as `p_subject_two's level.  Otherwise, it logs a permission
 * failure on operation `op_s' and returns 0.
 *
 * This function is used to mediate pgrp changes.
 *
 */

int
mediate_subject_level_subject(const char *op_s,
    const lomac_subject_t *p_subject_one, level_t level_one,
    lomac_subject_t *p_subject_two) {

	lattr_t lattr_two;     /* lattr of `p_subject_two' */
	int ret_val;           /* result to return to caller */

#ifdef NO_MEDIATION
	ret_val = 1;        /* no denials, just logging */
#else
	ret_val = 0;        /* pessimistically assume deny */
#endif

	get_subject_lattr(p_subject_two, &lattr_two);

	if (lattr_two.level <= level_one) {
		ret_val = 1;      /* OK, allow */
	} else if (lomac_verbose_demote_deny) {
		lomac_log_t *logmsg = log_start();

		log_append_string(logmsg, "LOMAC: denied level-");
		log_append_int(logmsg, level_one);
		log_append_string(logmsg, " proc ");
		log_append_subject_id(logmsg, p_subject_one);
		log_append_string(logmsg, " ");
		log_append_string(logmsg, op_s);
		log_append_string(logmsg, " to level-");
		log_append_int(logmsg, lattr_two.level);
		log_append_string(logmsg, " proc ");
		log_append_subject_id(logmsg, p_subject_two);
		log_print(logmsg);
	}
	return (ret_val);
} /* mediate_subject_subject() */

/* mediate_subject_object()
 *
 * in:     op_s      - string describing operation, like "write" or "writev"
 *         p_subject - subject trying to operate on `p_object'.
 *         p_object  - object that `p_subject' is trying to operate on.
 * out:    nothing
 * return: value    condition
 *         -----    ---------
 *           0      Caller should prevent operation
 *           1      Caller should permit operation
 * 
 * This function returns 1 if the level of `p_object' is less than or
 * equal to the level of `p_subject'.  Otherwise, it returns 0 and
 * logs a permission denial on `op_s'.
 *
 * This function allows LOMAC to mediate write and writev system calls.
 *
 */

int
mediate_subject_object(const char *op_s, lomac_subject_t *p_subject,
    const lomac_object_t *p_object) {
	lattr_t subject_lattr;     /* lattr of `p_subject' */
	lattr_t object_lattr;      /* lattr of `p_object' */
	int ret_val;               /* value to return to caller */

#ifdef NO_MEDIATION
	ret_val = 1;           /* allow operation regardless of decision */
#else
	ret_val = 0;           /* pessimistically assume deny */
#endif

	/* Get the lattrs of `p_subject' and `p_object' so we can compare them. */
	get_subject_lattr(p_subject, &subject_lattr);
	get_object_lattr(p_object,  &object_lattr);

	/*
	 * If `p_subject's level is less than `p_object's level,
	 * we indicate that the operation must not be allowed.
	 */

	if (!lomac_must_deny(&subject_lattr, &object_lattr) ||
	    object_lattr.flags & LOMAC_ATTR_LOWWRITE) {
		ret_val = 1;         /* allow operation */
	} else if (lomac_verbose_demote_deny) {
		lomac_log_t *logmsg = log_start();
		log_append_string(logmsg, "LOMAC: level-");
		log_append_int(logmsg, subject_lattr.level);
		log_append_string(logmsg, " proc ");
		log_append_subject_id(logmsg, p_subject);
		log_append_string(logmsg, " denied ");
		log_append_string(logmsg, op_s);
		log_append_string(logmsg, " to level-");
		log_append_int(logmsg, object_lattr.level);
		log_append_string(logmsg, " object ");
		log_append_object_id(logmsg, p_object);
		log_append_string(logmsg, "\n");
		log_print(logmsg);
	}
	return (ret_val);
} /* mediate_subject_object() */


/* mediate_subject_object_open()
 *
 * in:     p_subject - subject trying to operate on `p_object'.
 *         p_object  - object that `p_subject' is trying to operate on.
 * out:    nothing
 * return: value    condition
 *         -----    ---------
 *           0      Caller should prevent operation
 *           1      Caller should permit operation
 * 
 * This function returns 1 if the level of `p_object' is less than or
 * equal to the level of `p_subject'.  Otherwise, it returns 0 and
 * logs a permission denial on `op_s'.
 *
 * This function allows LOMAC to mediate open system calls.
 *
 */

int
mediate_subject_object_open(lomac_subject_t *p_subject, 
    const lomac_object_t *p_object) {
	lattr_t subject_lattr;     /* lattr of `p_subject' */
	lattr_t object_lattr;      /* lattr of `p_object' */
	int ret_val;               /* value to return to caller */

#ifdef NO_MEDIATION
	ret_val = 1;           /* allow operation regardless of decision */
#else
	ret_val = 0;           /* pessimistically assume deny */
#endif

	/* Get the lattrs of `p_subject' and `p_object' so we can compare them. */
	get_subject_lattr(p_subject, &subject_lattr);
	get_object_lattr(p_object,  &object_lattr);

	/*
	 * If `p_subject's level is less than `p_object's level,
	 * we must indicate that the operation should not be allowed.
	 */
	if (lomac_must_deny(&subject_lattr, &object_lattr) &&
	    object_lattr.flags & LOMAC_ATTR_LOWNOOPEN) {
		if (lomac_verbose_demote_deny) {
			lomac_log_t *logmsg = log_start();

			log_append_string(logmsg, "LOMAC: level-");
			log_append_int(logmsg, subject_lattr.level);
			log_append_string(logmsg, " proc ");
			log_append_subject_id(logmsg, p_subject);
			log_append_string(logmsg, " denied open to level-");
			log_append_int(logmsg, object_lattr.level);
			log_append_string(logmsg, " object ");
			log_append_object_id(logmsg, p_object);
			log_append_string(logmsg, "\n");
			log_print(logmsg);
		}
	} else {
		ret_val = 1;         /* allow operation */
	} /* if/else allow/deny */
	return (ret_val);
} /* mediate_subject_object() */


/* mediate_subject_at_level()
 *
 * in:     op_s         - name of operation being mediated
 *         p_subject    - subject whose level we want to check
 *         target_level - level to compare to `p_subject's level
 *         
 * out:    nothing
 * return: value   condition
 *         -----   ---------
 *           0     `p_subject' is not at `target_level'
 *           1     `p_subject' is at `target_level'
 *
 * This function provides a predicate for determining whether or not
 * `p_subject' is at the level specified by `target_level'.  This
 * function compares `p_subject's level to `target_level'.  If the
 * levels match, it retruns 1.  Otherwise, it logs a permission denial
 * on `op_s' and returns 0.
 *
 */

int
mediate_subject_at_level(const char *op_s, lomac_subject_t *p_subject,
    const level_t target_level) {
	lattr_t subject_lattr;   /* lattr of `p_subject' */
	int ret_val;             /* value returned to caller */

#ifdef NO_MEDIATION
	ret_val = 1;           /* allow operation regardless of decision */
#else
	ret_val = 0;           /* pessimistically assume deny */
#endif

	/* Make `subject_lattr' the lattr of `p_subject'. */
	get_subject_lattr(p_subject, &subject_lattr);

	/* compare with `target_lattr */
	if (subject_lattr.level == target_level) {
		ret_val = 1;    /* allow operation */
	} else if (lomac_verbose_demote_deny) {
		lomac_log_t *logmsg = log_start();

		log_append_string(logmsg, "LOMAC: denied level-");
		log_append_int(logmsg, subject_lattr.level);
		log_append_string(logmsg, " proc ");
		log_append_subject_id(logmsg, p_subject);
		log_append_string(logmsg, "'s ");
		log_append_string(logmsg, op_s);
		log_append_string(logmsg, ".\n");
		log_print(logmsg);
	}
	return (ret_val);
} /* mediate_subject_at_level() */
