/*
%%% copyright-cmetz-97
This software is Copyright 1997-1998 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

*/

#include <port_before.h>
#include <netdb.h>
#include <errno.h>
#include <port_after.h>

char *
gai_strerror(int errnum) {
	switch(errnum) {
	case 0:
		return "no error";
	case EAI_BADFLAGS:
		return "invalid value for ai_flags";
	case EAI_NONAME:
		return "name or service is not known";
	case EAI_AGAIN:
		return "temporary failure in name resolution";
	case EAI_FAIL:
		return "non-recoverable failure in name resolution";
	case EAI_NODATA:
		return "no address associated with name";
	case EAI_FAMILY:
		return "ai_family not supported";
	case EAI_SOCKTYPE:
		return "ai_socktype not supported";
	case EAI_SERVICE:
		return "service not supported for ai_socktype";
	case EAI_ADDRFAMILY:
		return "address family for name not supported";
	case EAI_MEMORY:
		return "memory allocation failure";
	case EAI_SYSTEM:
		return "system error";
	default:
		return "unknown error";
	};
}
