/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: vsprintf.c,v 1.21 2001/09/11 04:04:49 gshapiro Exp $")
#include <limits.h>
#include <sm/io.h>
#include "local.h"

/*
**  SM_VSPRINTF -- format data for "output" into a string
**
**	Assigned 'str' to a "fake" file pointer. This allows common
**	o/p formatting function sm_vprintf() to be used.
**
**	Parameters:
**		str -- location for output
**		fmt -- format directives
**		ap -- data unit vectors for use by 'fmt'
**
**	Results:
**		result from sm_io_vfprintf()
**
**	Side Effects:
**		Quietly limits the size to INT_MAX though this may
**		not prevent SEGV's.
*/

int
sm_vsprintf(str, fmt, ap)
	char *str;
	const char *fmt;
	SM_VA_LOCAL_DECL
{
	int ret;
	SM_FILE_T fake;

	fake.sm_magic = SmFileMagic;
	fake.f_file = -1;
	fake.f_flags = SMWR | SMSTR;
	fake.f_bf.smb_base = fake.f_p = (unsigned char *)str;
	fake.f_bf.smb_size = fake.f_w = INT_MAX;
	fake.f_timeout = SM_TIME_FOREVER;
	fake.f_timeoutstate = SM_TIME_BLOCK;
	fake.f_close = NULL;
	fake.f_open = NULL;
	fake.f_read = NULL;
	fake.f_write = NULL;
	fake.f_seek = NULL;
	fake.f_setinfo = fake.f_getinfo = NULL;
	fake.f_type = "sm_vsprintf:fake";
	ret = sm_io_vfprintf(&fake, SM_TIME_FOREVER, fmt, ap);
	*fake.f_p = '\0';
	return ret;
}
