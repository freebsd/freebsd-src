/*
 * Copyright (c) 1998, 1999 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: sysexits.c,v 8.25 1999/09/23 19:59:24 ca Exp $";
#endif /* ! lint */

#include <sendmail.h>

/*
**  SYSEXITS.C -- error messages corresponding to sysexits.h
**
**	If the first character of the string is a colon, interpolate
**	the current errno after the rest of the string.
*/

char *SysExMsg[] =
{
	/* 64 USAGE */		" 500 5.0.0 Bad usage",
	/* 65 DATAERR */	" 501 5.6.0 Data format error",
	/* 66 NOINPUT */	":550 5.3.0 Cannot open input",
	/* 67 NOUSER */		" 550 5.1.1 User unknown",
	/* 68 NOHOST */		" 550 5.1.2 Host unknown",
	/* 69 UNAVAILABLE */	" 554 5.0.0 Service unavailable",
	/* 70 SOFTWARE */	":554 5.3.0 Internal error",
	/* 71 OSERR */		":451 4.0.0 Operating system error",
	/* 72 OSFILE */		":554 5.3.5 System file missing",
	/* 73 CANTCREAT */	":550 5.0.0 Can't create output",
	/* 74 IOERR */		":451 4.0.0 I/O error",
	/* 75 TEMPFAIL */	" 450 4.0.0 Deferred",
	/* 76 PROTOCOL */	" 554 5.5.0 Remote protocol error",
	/* 77 NOPERM */		":550 5.0.0 Insufficient permission",
	/* 78 CONFIG */		" 554 5.3.5 Local configuration error",
};

int N_SysEx = sizeof(SysExMsg) / sizeof(SysExMsg[0]);

static char *SysExitMsg[] =
{
	"command line usage error",
	"data format error",
	"cannot open input",
	"addressee unknown",
	"host name unknown",
	"service unavailable",
	"internal software error",
	"system error (e.g., can't fork)",
	"critical OS file missing",
	"can't create (user) output file",
	"input/output error",
	"temp failure; user is invited to retry",
	"remote error in protocol",
	"permission denied",
	"configuration error"
};

/*
**  DSNTOEXITSTAT -- convert DSN-style error code to EX_ style.
**
**	Parameters:
**		dsncode -- the text of the DSN-style code.
**
**	Returns:
**		The corresponding exit status.
*/

int
dsntoexitstat(dsncode)
	char *dsncode;
{
	int code2, code3;

	/* first the easy cases.... */
	if (*dsncode == '2')
		return EX_OK;
	if (*dsncode == '4')
		return EX_TEMPFAIL;

	/* now decode the other two field parts */
	if (*++dsncode == '.')
		dsncode++;
	code2 = atoi(dsncode);
	while (*dsncode != '\0' && *dsncode != '.')
		dsncode++;
	if (*dsncode != '\0')
		dsncode++;
	code3 = atoi(dsncode);

	/* and do a nested switch to work them out */
	switch (code2)
	{
	  case 0:	/* Other or Undefined status */
		return EX_UNAVAILABLE;

	  case 1:	/* Address Status */
		switch (code3)
		{
		  case 0:	/* Other Address Status */
			return EX_DATAERR;

		  case 1:	/* Bad destination mailbox address */
		  case 6:	/* Mailbox has moved, No forwarding address */
			return EX_NOUSER;

		  case 2:	/* Bad destination system address */
		  case 8:	/* Bad senders system address */
			return EX_NOHOST;

		  case 3:	/* Bad destination mailbox address syntax */
		  case 7:	/* Bad senders mailbox address syntax */
			return EX_USAGE;

		  case 4:	/* Destination mailbox address ambiguous */
			return EX_UNAVAILABLE;

		  case 5:	/* Destination address valid */
			return EX_OK;
		}
		break;

	  case 2:	/* Mailbox Status */
		switch (code3)
		{
		  case 0:	/* Other or Undefined mailbox status */
		  case 1:	/* Mailbox disabled, not accepting messages */
		  case 2:	/* Mailbox full */
		  case 4:	/* Mailing list expansion problem */
			return EX_UNAVAILABLE;

		  case 3:	/* Message length exceeds administrative lim */
			return EX_DATAERR;
		}
		break;

	  case 3:	/* System Status */
		return EX_OSERR;

	  case 4:	/* Network and Routing Status */
		switch (code3)
		{
		  case 0:	/* Other or undefined network or routing stat */
			return EX_IOERR;

		  case 1:	/* No answer from host */
		  case 3:	/* Routing server failure */
		  case 5:	/* Network congestion */
			return EX_TEMPFAIL;

		  case 2:	/* Bad connection */
			return EX_IOERR;

		  case 4:	/* Unable to route */
			return EX_PROTOCOL;

		  case 6:	/* Routing loop detected */
			return EX_CONFIG;

		  case 7:	/* Delivery time expired */
			return EX_UNAVAILABLE;
		}
		break;

	  case 5:	/* Protocol Status */
		return EX_PROTOCOL;

	  case 6:	/* Message Content or Media Status */
		return EX_UNAVAILABLE;

	  case 7:	/* Security Status */
		return EX_DATAERR;
	}
	return EX_CONFIG;
}

/*
**  EXITSTAT -- convert EX_ value to error text.
**
**	Parameters:
**		excode -- rstatus which might consists of an EX_* value.
**
**	Returns:
**		The corresponding error text or the original string.
*/

char *
exitstat(excode)
	char *excode;
{
	char *c;
	int i;

	if (excode == NULL || *excode == '\0')
		return excode;
	i = 0;
	for (c = excode; *c != '\0'; c++)
	{
		if (isascii(*c) && isdigit(*c))
			i = i * 10 + (*c - '0');
		else
			return excode;
	}
	i -= EX__BASE;
	if (i >= 0 && i <= N_SysEx)
		return SysExitMsg[i];
	return excode;
}
