/*
 * Copyright (c) 2003, Trent Nelson, <trent@arpa.com>.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef _CONVTBL_H_
#define _CONVTBL_H_

#include <sys/types.h>

#define BITS	(1)
#define BYTES	(1)
#define KILO	(1024)
#define	MEGA	(KILO * 1024)
#define GIGA	(MEGA * 1024)

#define SC_BYTE		(0)
#define SC_KILOBYTE	(1)
#define SC_MEGABYTE	(2)
#define SC_GIGABYTE	(3)
#define SC_BIT		(4)
#define	SC_KILOBIT	(5)
#define	SC_MEGABIT	(6)
#define SC_GIGABIT	(7)
#define SC_AUTO		(8)

#define BIT	(8)
#define BYTE	(1)

struct convtbl {
	u_int	 mul;
	u_int	 scale;
	const char	*str;
};

extern	struct convtbl convtbl[];

extern	double 	 convert(const u_long, const u_int);
extern	const char	*get_string(const u_long, const u_int);

#endif		/* ! _CONVTBL_H_ */
/*
 * Copyright (c) 2003, Trent Nelson, <trent@arpa.com>.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#ifndef _CONVTBL_H_
#define _CONVTBL_H_

#include <sys/types.h>

#define BITS	(1)
#define BYTES	(1)
#define KILO	(1024)
#define	MEGA	(KILO * 1024)
#define GIGA	(MEGA * 1024)

#define SC_BYTE		(0)
#define SC_KILOBYTE	(1)
#define SC_MEGABYTE	(2)
#define SC_GIGABYTE	(3)
#define SC_BIT		(4)
#define	SC_KILOBIT	(5)
#define	SC_MEGABIT	(6)
#define SC_GIGABIT	(7)
#define SC_AUTO		(8)

#define BIT	(8)
#define BYTE	(1)

struct convtbl {
	u_int	 mul;
	u_int	 scale;
	char	*str;
};

extern	struct convtbl convtbl[];

extern	double 	 convert(const u_long, const u_int);
extern	char	*get_string(const u_long, const u_int);

#endif		/* ! _CONVTBL_H_ */
