/*-
 * Copyright 2018 Aniket Pandey
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
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */


#ifndef _UTILS_H_
#define _UTILS_H_

#include <poll.h>
#include <stdio.h>
#include <stdbool.h>
#include <bsm/audit.h>

void check_audit(struct pollfd [], const char *, FILE *);
FILE *setup(struct pollfd [], const char *);
void cleanup(void);
void skip_if_extattr_not_supported(const char *);

#define REQUIRE_EXTATTR_SUCCESS(call)						\
	({									\
		errno = 0; /* Reset errno before call */			\
		ssize_t result = (call);					\
		if (result == -1) {						\
			atf_tc_fail_requirement(__FILE__, __LINE__,		\
			    "%s failed with errno %d (%s)", #call, errno,	\
			    strerror(errno));					\
		}								\
		result;								\
	})

#define REQUIRE_EXTATTR_RESULT(_expected, expr)					\
	do {									\
		ssize_t expected = (_expected);					\
		ssize_t _result = REQUIRE_EXTATTR_SUCCESS(expr);		\
		ATF_REQUIRE_EQ_MSG(expected, _result, "%s: %zd != %zd", #expr,	\
		    expected, _result);						\
	} while (0)

#endif  /* _SETUP_H_ */
