/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
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
 * This file defines the interface between libgeom and the geom control device
 * in the kernel, the interfaces defined herein are to be considered private
 * and may only be used by libgeom.  Applications wishing to interact with
 * the geom subsystem must use libgeoms published APIs.
 *
 * $FreeBSD$
 */

#ifndef _GEOM_GEOM_EXT_H_
#define _GEOM_GEOM_EXT_H_

#include <sys/ioccom.h>

struct geom_ctl_req_arg {
	u_int				nlen;
	char				*name;
	off_t				offset;
	int				len;
	void				*value;
};

struct geom_ctl_req {
	u_int				version;
	u_int				serial;
	enum geom_ctl_request		request;
	u_int				narg;
	struct geom_ctl_req_arg		*arg;
	u_int				lerror;
	char				*error;
	struct geom_ctl_req_table	*reqt;
};

#define GEOM_CTL	_IOW('G', GEOM_CTL_VERSION, struct geom_ctl_req)

#endif /* _GEOM_GEOM_EXT_H_ */
