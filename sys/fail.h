/*-
 * Copyright (c) 2009 Isilon Inc http://www.isilon.com/
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
/**
 * @file
 *
 * Main header for failpoint facility.
 */
#ifndef _SYS_FAIL_H_
#define _SYS_FAIL_H_

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/linker_set.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

/**
 * Failpoint return codes, used internally.
 * @ingroup failpoint_private
 */
enum fail_point_return_code {
	FAIL_POINT_RC_CONTINUE = 0,	/**< Continue with normal execution */
	FAIL_POINT_RC_RETURN,		/**< FP evaluated to 'return' */
	FAIL_POINT_RC_QUEUED,		/**< sleep_fn will be called */
};

struct fail_point_entry;
TAILQ_HEAD(fail_point_entries, fail_point_entry);
/**
 * Internal failpoint structure, tracking all the current details of the
 * failpoint.  This structure is the core component shared between the
 * failure-injection code and the user-interface.
 * @ingroup failpoint_private
 */
struct fail_point {
	const char *fp_name;		/**< name of fail point */
	const char *fp_location;	/**< file:line of fail point */
	struct fail_point_entries fp_entries;	/**< list of entries */
	int fp_flags;
	void (*fp_sleep_fn)(void *);	/**< Function to call at end of
					 * sleep for sleep failpoints */
	void *fp_sleep_arg;		/**< Arg for sleep_fn */
};

#define	FAIL_POINT_DYNAMIC_NAME	0x01	/**< Must free name on destroy */

__BEGIN_DECLS

/* Private failpoint eval function -- use fail_point_eval() instead. */
enum fail_point_return_code fail_point_eval_nontrivial(struct fail_point *,
	int *ret);

/**
 * @addtogroup failpoint
 * @{
 */
/*
 * Initialize a fail-point.  The name is formed in printf-like fashion
 * from "fmt" and the subsequent arguments.
 * Pair with fail_point_destroy().
 */
void fail_point_init(struct fail_point *, const char *fmt, ...)
    __printflike(2, 3);

/**
 * Set the sleep function for a fail point
 * If sleep_fn is specified, then FAIL_POINT_SLEEP will result in a
 * (*fp->sleep_fn)(fp->sleep_arg) call by the timer thread.  Otherwise,
 * if sleep_fn is NULL (default), then FAIL_POINT_SLEEP will result in the
 * fail_point_eval() call sleeping.
 */
static __inline void
fail_point_sleep_set_func(struct fail_point *fp, void (*sleep_fn)(void *))
{
	fp->fp_sleep_fn = sleep_fn;
}

/**
 * Set the argument for the sleep function for a fail point
 */
static __inline void
fail_point_sleep_set_arg(struct fail_point *fp, void *sleep_arg)
{
	fp->fp_sleep_arg = sleep_arg;
}

/**
 * Free the resources used by a fail-point.  Pair with fail_point_init().
 */
void fail_point_destroy(struct fail_point *);

/**
 * Evaluate a failpoint.
 */
static __inline enum fail_point_return_code
fail_point_eval(struct fail_point *fp, int *ret)
{
	if (TAILQ_EMPTY(&fp->fp_entries)) {
		return (FAIL_POINT_RC_CONTINUE);
	}
	return (fail_point_eval_nontrivial(fp, ret));
}

__END_DECLS

/* Declare a fail_point and its sysctl in a function. */
#define	_FAIL_POINT_NAME(name)	_fail_point_##name
#define	_FAIL_POINT_LOCATION()	"(" __FILE__ ":" __XSTRING(__LINE__) ")"

/**
 * Instantiate a failpoint which returns "value" from the function when triggered.
 * @param parent  The parent sysctl under which to locate the sysctl
 * @param name    The name of the failpoint in the sysctl tree (and printouts)
 * @return        Instantly returns the return("value") specified in the
 *                failpoint, if triggered.
 */
#define KFAIL_POINT_RETURN(parent, name) \
	KFAIL_POINT_CODE(parent, name, return RETURN_VALUE)

/**
 * Instantiate a failpoint which returns (void) from the function when triggered.
 * @param parent  The parent sysctl under which to locate the sysctl
 * @param name    The name of the failpoint in the sysctl tree (and printouts)
 * @return        Instantly returns void, if triggered in the failpoint.
 */
#define KFAIL_POINT_RETURN_VOID(parent, name) \
	KFAIL_POINT_CODE(parent, name, return)

/**
 * Instantiate a failpoint which sets an error when triggered.
 * @param parent     The parent sysctl under which to locate the sysctl
 * @param name       The name of the failpoint in the sysctl tree (and printouts)
 * @param error_var  A variable to set to the failpoint's specified
 *                   return-value when triggered
 */
#define KFAIL_POINT_ERROR(parent, name, error_var) \
	KFAIL_POINT_CODE(parent, name, (error_var) = RETURN_VALUE)

/**
 * Instantiate a failpoint which sets an error and then goes to a
 * specified label in the function when triggered.
 * @param parent     The parent sysctl under which to locate the sysctl
 * @param name       The name of the failpoint in the sysctl tree (and printouts)
 * @param error_var  A variable to set to the failpoint's specified
 *                   return-value when triggered
 * @param label      The location to goto when triggered.
 */
#define KFAIL_POINT_GOTO(parent, name, error_var, label) \
	KFAIL_POINT_CODE(parent, name, (error_var) = RETURN_VALUE; goto label)

/**
 * Instantiate a failpoint which runs arbitrary code when triggered.
 * @param parent     The parent sysctl under which to locate the sysctl
 * @param name       The name of the failpoint in the sysctl tree
 *		     (and printouts)
 * @param code       The arbitrary code to run when triggered.  Can reference
 *                   "RETURN_VALUE" if desired to extract the specified
 *                   user return-value when triggered.  Note that this is
 *                   implemented with a do-while loop so be careful of
 *                   break and continue statements.
 */
#define KFAIL_POINT_CODE(parent, name, code)				\
do {									\
	int RETURN_VALUE;						\
	static struct fail_point _FAIL_POINT_NAME(name) = {		\
		#name,							\
		_FAIL_POINT_LOCATION(),					\
		TAILQ_HEAD_INITIALIZER(_FAIL_POINT_NAME(name).fp_entries), \
		0,							\
		NULL, NULL,						\
	};								\
	SYSCTL_OID(parent, OID_AUTO, name,				\
	    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,		\
	    &_FAIL_POINT_NAME(name), 0, fail_point_sysctl,		\
	    "A", "");							\
									\
	if (__predict_false(						\
	    fail_point_eval(&_FAIL_POINT_NAME(name), &RETURN_VALUE))) {	\
									\
		code;							\
									\
	}								\
} while (0)


/**
 * @}
 * (end group failpoint)
 */

#ifdef _KERNEL
int fail_point_sysctl(SYSCTL_HANDLER_ARGS);

/* The fail point sysctl tree. */
SYSCTL_DECL(_debug_fail_point);
#define	DEBUG_FP	_debug_fail_point
#endif

#endif /* _SYS_FAIL_H_ */
