/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * NIS interface routines for chpass
 * 
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Center for Telecommunications Research
 * Columbia University, New York City
 *
 * $FreeBSD$
 */

#ifdef YP
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
/* Four possible return codes from use_yp() */
#define USER_UNKNOWN 0
#define USER_YP_ONLY 1
#define USER_LOCAL_ONLY 2
#define USER_YP_AND_LOCAL 3

extern	int		force_old;
extern	int		_use_yp;
extern	int		suser_override;
extern	struct passwd	local_password;
extern	struct passwd	yp_password;
extern	void		copy_yp_pass __P(( char *, int, int ));
extern	char		*yp_domain;
extern	char		*yp_server;
extern	void		yp_submit	__P(( struct passwd * ));
extern	int		use_yp		__P(( char * , uid_t , int ));
extern	char		*get_yp_master	__P(( int ));
extern	int		yp_in_pw_file;

/*
 * Yucky.
 */
#define GETPWUID(X) \
	_use_yp = use_yp(NULL, X, 1);					\
									\
	if (_use_yp == USER_UNKNOWN) {					\
		errx(1, "unknown user: uid %u", X);			\
	}								\
									\
	if (_use_yp == USER_YP_ONLY) {					\
		if (!force_local) {					\
			_use_yp = 1;					\
			pw = (struct passwd *)&yp_password;		\
		} else							\
			errx(1, "unknown local user: uid %u", X);	\
	} else if (_use_yp == USER_LOCAL_ONLY) {			\
		if (!force_yp) {					\
			_use_yp = 0;					\
			pw = (struct passwd *)&local_password;		\
		} else							\
			errx(1, "unknown NIS user: uid %u", X);		\
	} else if (_use_yp == USER_YP_AND_LOCAL) {			\
		if (!force_local && (force_yp || yp_in_pw_file)) {	\
			_use_yp = 1;					\
			pw = (struct passwd *)&yp_password;		\
		} else {						\
			_use_yp = 0;					\
			pw = (struct passwd *)&local_password;		\
		}							\
	}

#define GETPWNAM(X) \
	_use_yp = use_yp(X, 0, 0);					\
									\
	if (_use_yp == USER_UNKNOWN) {					\
		errx(1, "unknown user: %s", X);				\
	}								\
									\
	if (_use_yp == USER_YP_ONLY) {					\
		if (!force_local) {					\
			_use_yp = 1;					\
			pw = (struct passwd *)&yp_password;		\
		} else							\
			errx(1, "unknown local user: %s.", X);		\
	} else if (_use_yp == USER_LOCAL_ONLY) {			\
		if (!force_yp) {					\
			_use_yp = 0;					\
			pw = (struct passwd *)&local_password;		\
		} else							\
			errx(1, "unknown NIS user: %s.", X);		\
	} else if (_use_yp == USER_YP_AND_LOCAL) {			\
		if (!force_local && (force_yp || yp_in_pw_file)) {	\
			_use_yp = 1;					\
			pw = (struct passwd *)&yp_password;		\
		} else {						\
			_use_yp = 0;					\
			pw = (struct passwd *)&local_password;		\
		}							\
	}

#endif /* YP */
