/*===========================================================================
=	Novell Standard C Library for NetWare Loadable Modules
=
=	Unpublished Copyright (C) 1993 by Novell, Inc. All rights reserved.
=
=	No part of this file may be duplicated, revised, translated, localized or
=	modified in any manner or compiled, linked or uploaded or downloaded to or
=	from any computer system without the prior written consent of Novell, Inc.
==============================================================================
=	The object produced by compiling this file is for use by the client of this
=	library and is not linked in;  Prelude.Obj is therefore one of the files to
=	be distributed with CLib.NLM and its headers.
==============================================================================
*/

#include <stddef.h>
#if defined(__netware__) && defined(__i386__)
#define TERMINATE_BY_UNLOAD	5
#else
#include <nwpre.h>
#endif
/*#include "libhooks.h"*/

extern int main (int, char **);

static int	NLMID;


void _Stop( void )
{
	_TerminateNLM(NLMID, NULL, TERMINATE_BY_UNLOAD);
}

int _cstart_( void )
{
   return _SetupArgv(main);
}

int _Prelude
(
	int	NLMHandle,
	int	initErrorScreenID,
	char	*commandLine,
	char	*loadDirectoryPath,
	int	uninitializedDataLength,
	int	NLMFileHandle,
	int	(*readRoutineP)(),
	int	customDataOffset,
	int	customDataSize
)
{
	int	rc;

   rc = _StartNLM(NLMHandle,
					initErrorScreenID,
					commandLine,
					loadDirectoryPath,
					uninitializedDataLength,
					NLMFileHandle,
					readRoutineP,
					customDataOffset,
					customDataSize,
               &NLMID,
					_cstart_);

	return rc;
}
