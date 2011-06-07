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
SM_RCSID("@(#)$Id: clrerr.c,v 1.13 2001/09/11 04:04:48 gshapiro Exp $")
#include <sm/io.h>
#include <sm/assert.h>
#include "local.h"

/*
**  SM_IO_CLEARERR -- public function to clear a file pointer's error status
**
**	Parameters:
**		fp -- the file pointer
**
**	Returns:
**		nothing.
*/
#undef	sm_io_clearerr

void
sm_io_clearerr(fp)
	SM_FILE_T *fp;
{
	SM_REQUIRE_ISA(fp, SmFileMagic);

	sm_clearerr(fp);
}
