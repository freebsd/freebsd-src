/*
 * Copyright (c) 2000-2002 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Donn Seeley at UUNET Technologies, Inc.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: vsscanf.c,v 1.23 2002/02/01 02:28:00 ca Exp $")
#include <string.h>
#include <sm/io.h>

/*
**  SM_EOFREAD -- dummy read function for faked file below
**
**	Parameters:
**		fp -- file pointer
**		buf -- location to place read data
**		len -- number of bytes to read
**
**	Returns:
**		0 (zero) always
*/

/* type declaration for later use */
static ssize_t sm_eofread __P((SM_FILE_T *, char *, size_t));

/* ARGSUSED0 */
static ssize_t
sm_eofread(fp, buf, len)
	SM_FILE_T *fp;
	char *buf;
	size_t len;
{
	return 0;
}

/*
**  SM_VSSCANF -- scan a string to find data units
**
**	Parameters:
**		str -- strings containing data
**		fmt -- format directive for finding data units
**		ap -- memory locations to place format found data units
**
**	Returns:
**		Failure: SM_IO_EOF
**		Success: number of data units found
**
**	Side Effects:
**		Attempts to strlen() 'str'; if not a '\0' terminated string
**			then the call may SEGV/fail.
**		Faking the string 'str' as a file.
*/

int
sm_vsscanf(str, fmt, ap)
	const char *str;
	const char *fmt;
	SM_VA_LOCAL_DECL
{
	SM_FILE_T fake;

	fake.sm_magic = SmFileMagic;
	fake.f_timeout = SM_TIME_FOREVER;
	fake.f_timeoutstate = SM_TIME_BLOCK;
	fake.f_file = -1;
	fake.f_flags = SMRD;
	fake.f_bf.smb_base = fake.f_p = (unsigned char *)str;
	fake.f_bf.smb_size = fake.f_r = strlen(str);
	fake.f_read = sm_eofread;
	fake.f_ub.smb_base = NULL;
	fake.f_close = NULL;
	fake.f_open = NULL;
	fake.f_write = NULL;
	fake.f_seek = NULL;
	fake.f_setinfo = fake.f_getinfo = NULL;
	fake.f_type = "sm_vsscanf:fake";
	return sm_vfscanf(&fake, SM_TIME_FOREVER, fmt, ap);
}
