/* @(#)clnt_perror.c	2.1 88/07/29 4.0 RPCSRC */
/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the "Oracle America, Inc." nor the names of
 *       its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)clnt_perror.c 1.15 87/10/07 Copyr 1984 Sun Micro";
#endif

/*
 * clnt_perror.c
 */
#include "autoconf.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <gssrpc/types.h>
#include <gssrpc/auth.h>
#include <gssrpc/clnt.h>

#ifndef HAVE_STRERROR
#ifdef NEED_SYS_ERRLIST
extern char *sys_errlist[];
#endif
extern int sys_nerr;
#undef strerror
#define strerror(N) (((N) > 0 && (N) < sys_nerr) ? sys_errlist[N] : (char *)0)
#endif /* HAVE_STRERROR */
static char *auth_errmsg(enum auth_stat);



static char *buf;

static char *
get_buf(void)
{
	if (buf == NULL)
		buf = (char *)malloc(BUFSIZ);
	return (buf);
}

/*
 * Print reply error info
 */
char *
clnt_sperror(CLIENT *rpch, char *s)
{
	struct rpc_err e;
	void clnt_perrno();
	char *err;
	char *bufstart = get_buf();
	char *str = bufstart;
	char *strstart = str;
	char *strend;

	if (str == 0)
		return (0);
	strend = str + BUFSIZ;
	CLNT_GETERR(rpch, &e);

	strncpy (str, s, BUFSIZ - 1);
	str[BUFSIZ - 1] = 0;
	strncat (str, ": ", BUFSIZ - 1 - strlen (bufstart));
	str += strlen(str);
	strncat (str, clnt_sperrno(e.re_status), BUFSIZ - 1 - strlen (bufstart));
	strstart[BUFSIZ - 1] = '\0';
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
		/* 10 for the string */
		if (str - bufstart + 10 + strlen(strerror(e.re_errno)) < BUFSIZ)
		    (void) snprintf(str, strend-str, "; errno = %s",
				    strerror(e.re_errno));
		str += strlen(str);
		break;

	case RPC_VERSMISMATCH:
		/* 33 for the string, 22 for the numbers */
		if(str - bufstart + 33 + 22 < BUFSIZ)
		    (void) snprintf(str, strend-str,
				    "; low version = %lu, high version = %lu",
				    (u_long) e.re_vers.low,
				    (u_long) e.re_vers.high);
		str += strlen(str);
		break;

	case RPC_AUTHERROR:
		err = auth_errmsg(e.re_why);
		/* 8 for the string */
		if(str - bufstart + 8 < BUFSIZ)
		    (void) snprintf(str, strend-str, "; why = ");
		str += strlen(str);
		if (err != NULL) {
			if(str - bufstart + strlen(err) < BUFSIZ)
			    (void) snprintf(str, strend-str, "%s",err);
		} else {
		    /* 33 for the string, 11 for the number */
		    if(str - bufstart + 33 + 11 < BUFSIZ)
			(void) snprintf(str, strend-str,
					"(unknown authentication error - %d)",
					(int) e.re_why);
		}
		str += strlen(str);
		break;

	case RPC_PROGVERSMISMATCH:
		/* 33 for the string, 22 for the numbers */
		if(str - bufstart + 33 + 22 < BUFSIZ)
		    (void) snprintf(str, strend-str,
				    "; low version = %lu, high version = %lu",
				    (u_long) e.re_vers.low,
				    (u_long) e.re_vers.high);
		str += strlen(str);
		break;

	default:	/* unknown */
		/* 14 for the string, 22 for the numbers */
		if(str - bufstart + 14 + 22 < BUFSIZ)
		    (void) snprintf(str, strend-str,
				    "; s1 = %lu, s2 = %lu",
				    (u_long) e.re_lb.s1,
				    (u_long) e.re_lb.s2);
		str += strlen(str);
		break;
	}
	if (str - bufstart + 1 < BUFSIZ)
	    (void) snprintf(str, strend-str, "\n");
	return(strstart) ;
}

void
clnt_perror(CLIENT *rpch, char *s)
{
	(void) fprintf(stderr,"%s",clnt_sperror(rpch,s));
}


struct rpc_errtab {
	enum clnt_stat status;
	char *message;
};

static struct rpc_errtab  rpc_errlist[] = {
	{ RPC_SUCCESS,
		"RPC: Success" },
	{ RPC_CANTENCODEARGS,
		"RPC: Can't encode arguments" },
	{ RPC_CANTDECODERES,
		"RPC: Can't decode result" },
	{ RPC_CANTSEND,
		"RPC: Unable to send" },
	{ RPC_CANTRECV,
		"RPC: Unable to receive" },
	{ RPC_TIMEDOUT,
		"RPC: Timed out" },
	{ RPC_VERSMISMATCH,
		"RPC: Incompatible versions of RPC" },
	{ RPC_AUTHERROR,
		"RPC: Authentication error" },
	{ RPC_PROGUNAVAIL,
		"RPC: Program unavailable" },
	{ RPC_PROGVERSMISMATCH,
		"RPC: Program/version mismatch" },
	{ RPC_PROCUNAVAIL,
		"RPC: Procedure unavailable" },
	{ RPC_CANTDECODEARGS,
		"RPC: Server can't decode arguments" },
	{ RPC_SYSTEMERROR,
		"RPC: Remote system error" },
	{ RPC_UNKNOWNHOST,
		"RPC: Unknown host" },
	{ RPC_UNKNOWNPROTO,
		"RPC: Unknown protocol" },
	{ RPC_PMAPFAILURE,
		"RPC: Port mapper failure" },
	{ RPC_PROGNOTREGISTERED,
		"RPC: Program not registered"},
	{ RPC_FAILED,
		"RPC: Failed (unspecified error)"}
};


/*
 * This interface for use by clntrpc
 */
char *
clnt_sperrno(enum clnt_stat stat)
{
	unsigned int i;

	for (i = 0; i < sizeof(rpc_errlist)/sizeof(struct rpc_errtab); i++) {
		if (rpc_errlist[i].status == stat) {
			return (rpc_errlist[i].message);
		}
	}
	return ("RPC: (unknown error code)");
}

void
clnt_perrno(enum clnt_stat num)
{
	(void) fprintf(stderr,"%s",clnt_sperrno(num));
}


char *
clnt_spcreateerror(char *s)
{
	char *str = get_buf();
	char *strend;

	if (str == 0)
		return(0);
	strend = str+BUFSIZ;
	(void) snprintf(str, strend-str, "%s: ", s);
	str[BUFSIZ - 1] = '\0';
	(void) strncat(str, clnt_sperrno(rpc_createerr.cf_stat), BUFSIZ - 1);
	switch (rpc_createerr.cf_stat) {
	case RPC_PMAPFAILURE:
		(void) strncat(str, " - ", BUFSIZ - 1 - strlen(str));
		(void) strncat(str,
		    clnt_sperrno(rpc_createerr.cf_error.re_status),
		    BUFSIZ - 1 - strlen(str));
		break;

	case RPC_SYSTEMERROR:
		(void) strncat(str, " - ", BUFSIZ - 1 - strlen(str));
		{
		    const char *m = strerror(rpc_createerr.cf_error.re_errno);
		    if (m)
			(void) strncat(str, m, BUFSIZ - 1 - strlen(str));
		    else
			(void) snprintf(&str[strlen(str)], BUFSIZ - strlen(str),
					"Error %d",
					rpc_createerr.cf_error.re_errno);
		}
		break;

	case RPC_CANTSEND:
	case RPC_CANTDECODERES:
	case RPC_CANTENCODEARGS:
	case RPC_SUCCESS:
	case RPC_UNKNOWNPROTO:
	case RPC_PROGNOTREGISTERED:
	case RPC_FAILED:
	case RPC_UNKNOWNHOST:
	case RPC_CANTDECODEARGS:
	case RPC_PROCUNAVAIL:
	case RPC_PROGVERSMISMATCH:
	case RPC_PROGUNAVAIL:
	case RPC_AUTHERROR:
	case RPC_VERSMISMATCH:
	case RPC_TIMEDOUT:
	case RPC_CANTRECV:
	default:
	    break;
	}
	(void) strncat(str, "\n", BUFSIZ - 1 - strlen(str));
	return (str);
}

void
clnt_pcreateerror(char *s)
{
	(void) fprintf(stderr,"%s",clnt_spcreateerror(s));
}

struct auth_errtab {
	enum auth_stat status;
	char *message;
};

static struct auth_errtab auth_errlist[] = {
	{ AUTH_OK,
		"Authentication OK" },
	{ AUTH_BADCRED,
		"Invalid client credential" },
	{ AUTH_REJECTEDCRED,
		"Server rejected credential" },
	{ AUTH_BADVERF,
		"Invalid client verifier" },
	{ AUTH_REJECTEDVERF,
		"Server rejected verifier" },
	{ AUTH_TOOWEAK,
		"Client credential too weak" },
	{ AUTH_INVALIDRESP,
		"Invalid server verifier" },
	{ AUTH_FAILED,
		"Failed (unspecified error)" },
};

static char *
auth_errmsg(enum auth_stat stat)
{
	unsigned int i;

	for (i = 0; i < sizeof(auth_errlist)/sizeof(struct auth_errtab); i++) {
		if (auth_errlist[i].status == stat) {
			return(auth_errlist[i].message);
		}
	}
	return(NULL);
}
