/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)clnt_perror.c 1.15 87/10/07 Copyr 1984 Sun Micro";*/
/*static char *sccsid = "from: @(#)clnt_perror.c	2.1 88/07/29 4.0 RPCSRC";*/
static char *rcsid = "$FreeBSD: src/lib/libc/rpc/clnt_perror.c,v 1.11.2.1 2000/08/23 00:02:04 jhb Exp $";
#endif

/*
 * clnt_perror.c
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>

static char *auth_errmsg();
#define CLNT_PERROR_BUFLEN 256

static char *buf;

static char *
_buf()
{

	if (buf == 0)
		buf = (char *)malloc(CLNT_PERROR_BUFLEN);
	return (buf);
}

/*
 * Print reply error info
 */
char *
clnt_sperror(rpch, s)
	CLIENT *rpch;
	char *s;
{
	struct rpc_err e;
	char *err;
	char *str = _buf();
	char *strstart = str;

	if (str == 0)
		return (0);
	CLNT_GETERR(rpch, &e);

	(void) snprintf(str, CLNT_PERROR_BUFLEN, "%s: %s", s, clnt_sperrno(e.re_status));
	str += strlen(str);

	switch (e.re_status) {
	case RPC_SUCCESS:
	case RPC_CANTENCODEARGS:
	case RPC_CANTDECODERES:
	case RPC_TIMEDOUT:
	case RPC_PROGUNAVAIL:
	case RPC_PROCUNAVAIL:
	case RPC_CANTDECODEARGS:
	case RPC_SYSTEMERROR:
	case RPC_UNKNOWNHOST:
	case RPC_UNKNOWNPROTO:
	case RPC_PMAPFAILURE:
	case RPC_PROGNOTREGISTERED:
	case RPC_FAILED:
		break;

	case RPC_CANTSEND:
	case RPC_CANTRECV:
		(void) snprintf(str, CLNT_PERROR_BUFLEN - (str - strstart),
			"; errno = %s\n", strerror(e.re_errno));
		break;

	case RPC_VERSMISMATCH:
		(void) sprintf(str,
			"; low version = %lu, high version = %lu\n",
			(u_long)e.re_vers.low, (u_long)e.re_vers.high);
		break;

	case RPC_AUTHERROR:
		err = auth_errmsg(e.re_why);
		(void) sprintf(str,"; why = ");
		str += strlen(str);
		if (err != NULL) {
			(void) sprintf(str, "%s\n",err);
		} else {
			(void) sprintf(str,
				"(unknown authentication error - %d)\n",
				(int) e.re_why);
		}
		break;

	case RPC_PROGVERSMISMATCH:
		(void) sprintf(str,
			"; low version = %lu, high version = %lu\n",
			(u_long)e.re_vers.low, (u_long)e.re_vers.high);
		break;

	default:	/* unknown */
		(void) sprintf(str,
			"; s1 = %lu, s2 = %lu\n",
			(long)e.re_lb.s1, (long)e.re_lb.s2);
		break;
	}
	strstart[CLNT_PERROR_BUFLEN-2] = '\n';
	strstart[CLNT_PERROR_BUFLEN-1] = '\0';
	return(strstart) ;
}

void
clnt_perror(rpch, s)
	CLIENT *rpch;
	char *s;
{
	(void) fprintf(stderr,"%s\n",clnt_sperror(rpch,s));
}


static const char *const rpc_errlist[] = {
	"RPC: Success",				/*  0 - RPC_SUCCESS */
	"RPC: Can't encode arguments",		/*  1 - RPC_CANTENCODEARGS */
	"RPC: Can't decode result",		/*  2 - RPC_CANTDECODERES */
	"RPC: Unable to send",			/*  3 - RPC_CANTSEND */
	"RPC: Unable to receive",		/*  4 - RPC_CANTRECV */
	"RPC: Timed out",			/*  5 - RPC_TIMEDOUT */
	"RPC: Incompatible versions of RPC",	/*  6 - RPC_VERSMISMATCH */
	"RPC: Authentication error",		/*  7 - RPC_AUTHERROR */
	"RPC: Program unavailable",		/*  8 - RPC_PROGUNAVAIL */
	"RPC: Program/version mismatch",	/*  9 - RPC_PROGVERSMISMATCH */
	"RPC: Procedure unavailable",		/* 10 - RPC_PROCUNAVAIL */
	"RPC: Server can't decode arguments",	/* 11 - RPC_CANTDECODEARGS */
	"RPC: Remote system error",		/* 12 - RPC_SYSTEMERROR */
	"RPC: Unknown host",			/* 13 - RPC_UNKNOWNHOST */
	"RPC: Port mapper failure",		/* 14 - RPC_PMAPFAILURE */
	"RPC: Program not registered",		/* 15 - RPC_PROGNOTREGISTERED */
	"RPC: Failed (unspecified error)",	/* 16 - RPC_FAILED */
	"RPC: Unknown protocol"			/* 17 - RPC_UNKNOWNPROTO */
};


/*
 * This interface for use by clntrpc
 */
char *
clnt_sperrno(stat)
	enum clnt_stat stat;
{
	unsigned int errnum = stat;

	if (errnum < (sizeof(rpc_errlist)/sizeof(rpc_errlist[0])))
		return (char *)rpc_errlist[errnum];

	return ("RPC: (unknown error code)");
}

void
clnt_perrno(num)
	enum clnt_stat num;
{
	(void) fprintf(stderr,"%s\n",clnt_sperrno(num));
}


char *
clnt_spcreateerror(s)
	char *s;
{
	char *str = _buf();

	if (str == 0)
		return(0);
	switch (rpc_createerr.cf_stat) {
	case RPC_PMAPFAILURE:
		(void) snprintf(str, CLNT_PERROR_BUFLEN, "%s: %s - %s\n", s,
		    clnt_sperrno(rpc_createerr.cf_stat),
		    clnt_sperrno(rpc_createerr.cf_error.re_status));
		break;

	case RPC_SYSTEMERROR:
		(void) snprintf(str, CLNT_PERROR_BUFLEN, "%s: %s - %s\n", s,
		    clnt_sperrno(rpc_createerr.cf_stat),
		    strerror(rpc_createerr.cf_error.re_errno));
		break;
	default:
		(void) snprintf(str, CLNT_PERROR_BUFLEN, "%s: %s\n", s,
		clnt_sperrno(rpc_createerr.cf_stat));
		break;
	}
	str[CLNT_PERROR_BUFLEN-2] = '\n';
	str[CLNT_PERROR_BUFLEN-1] = '\0';
	return (str);
}

void
clnt_pcreateerror(s)
	char *s;
{
	(void) fprintf(stderr,"%s\n",clnt_spcreateerror(s));
}

static const char *const auth_errlist[] = {
	"Authentication OK",			/* 0 - AUTH_OK */
	"Invalid client credential",		/* 1 - AUTH_BADCRED */
	"Server rejected credential",		/* 2 - AUTH_REJECTEDCRED */
	"Invalid client verifier",		/* 3 - AUTH_BADVERF */
	"Server rejected verifier",		/* 4 - AUTH_REJECTEDVERF */
	"Client credential too weak",		/* 5 - AUTH_TOOWEAK */
	"Invalid server verifier",		/* 6 - AUTH_INVALIDRESP */
	"Failed (unspecified error)"		/* 7 - AUTH_FAILED */
};

static char *
auth_errmsg(stat)
	enum auth_stat stat;
{
	unsigned int errnum = stat;

	if (errnum < (sizeof(auth_errlist)/sizeof(auth_errlist[0])))
		return (char *)auth_errlist[errnum];

	return(NULL);
}
