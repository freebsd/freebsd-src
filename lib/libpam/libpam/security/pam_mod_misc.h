/*-
 * Copyright 1998 Juniper Networks, Inc.
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
 *	$FreeBSD$
 */

#ifndef PAM_MOD_MISC_H
#define PAM_MOD_MISC_H

#include <sys/cdefs.h>

/* Standard options
 */
enum opt { PAM_OPT_DEBUG, PAM_OPT_NO_WARN, PAM_OPT_ECHO_PASS,
	PAM_OPT_USE_FIRST_PASS, PAM_OPT_TRY_FIRST_PASS, PAM_OPT_USE_MAPPED_PASS,
	PAM_OPT_TRY_MAPPED_PASS, PAM_OPT_EXPOSE_ACCOUNT,
	PAM_OPT_STD_MAX /* XXX */ };

#define PAM_MAX_OPTIONS	32

struct opttab {
	const char *name;
	int value;
};

struct options {
	struct {
		const char *name;
		int bool;
		char *arg;
	} opt[PAM_MAX_OPTIONS];
};

__BEGIN_DECLS
void	pam_std_option(struct options *, struct opttab *, int, const char **);
int	pam_test_option(struct options *, enum opt, char **);
void	pam_set_option(struct options *, enum opt);
void	pam_clear_option(struct options *, enum opt);
void	_pam_verbose_error(pam_handle_t *, int, const char *,
		const char *, const char *, ...);
__END_DECLS

#define	PAM_LOG(args...)						\
	openpam_log(PAM_LOG_DEBUG, ##args)

#define PAM_RETURN(arg)							\
	return (arg)

#define PAM_VERBOSE_ERROR(args...)					\
	_pam_verbose_error(pamh, flags, __FILE__, __FUNCTION__, ##args)

#endif
