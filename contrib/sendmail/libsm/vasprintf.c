/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: vasprintf.c,v 1.26 2001/09/11 04:04:49 gshapiro Exp $")
#include <stdlib.h>
#include <errno.h>
#include <sm/io.h>
#include <sm/heap.h>
#include "local.h"

/*
**  SM_VASPRINTF -- printf to a dynamically allocated string
**
**  Write 'printf' output to a dynamically allocated string
**  buffer which is returned to the caller.
**
**	Parameters:
**		str -- *str receives a pointer to the allocated string
**		fmt -- format directives for printing
**		ap -- variable argument list
**
**	Results:
**		On failure, set *str to NULL, set errno, and return -1.
**
**		On success, set *str to a pointer to a nul-terminated
**		string buffer containing printf output,	and return the
**		length of the string (not counting the nul).
*/

#define SM_VA_BUFSIZE	128

int
sm_vasprintf(str, fmt, ap)
	char **str;
	const char *fmt;
	SM_VA_LOCAL_DECL
{
	int ret;
	SM_FILE_T fake;
	unsigned char *base;

	fake.sm_magic = SmFileMagic;
	fake.f_timeout = SM_TIME_FOREVER;
	fake.f_timeoutstate = SM_TIME_BLOCK;
	fake.f_file = -1;
	fake.f_flags = SMWR | SMSTR | SMALC;
	fake.f_bf.smb_base = fake.f_p = (unsigned char *)sm_malloc(SM_VA_BUFSIZE);
	if (fake.f_bf.smb_base == NULL)
		goto err2;
	fake.f_close = NULL;
	fake.f_open = NULL;
	fake.f_read = NULL;
	fake.f_write = NULL;
	fake.f_seek = NULL;
	fake.f_setinfo = fake.f_getinfo = NULL;
	fake.f_type = "sm_vasprintf:fake";
	fake.f_bf.smb_size = fake.f_w = SM_VA_BUFSIZE - 1;
	fake.f_timeout = SM_TIME_FOREVER;
	ret = sm_io_vfprintf(&fake, SM_TIME_FOREVER, fmt, ap);
	if (ret == -1)
		goto err;
	*fake.f_p = '\0';

	/* use no more space than necessary */
	base = (unsigned char *) sm_realloc(fake.f_bf.smb_base, ret + 1);
	if (base == NULL)
		goto err;
	*str = (char *)base;
	return ret;

err:
	if (fake.f_bf.smb_base != NULL)
	{
		sm_free(fake.f_bf.smb_base);
		fake.f_bf.smb_base = NULL;
	}
err2:
	*str = NULL;
	errno = ENOMEM;
	return -1;
}
