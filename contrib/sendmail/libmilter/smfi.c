/*
 *  Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: smfi.c,v 8.28.4.6 2000/06/28 23:48:56 gshapiro Exp $";
#endif /* ! lint */

#if _FFR_MILTER
#include "libmilter.h"
#include "sendmail/useful.h"

/*
**  SMFI_ADDHEADER -- send a new header to the MTA
**
**	Parameters:
**		ctx -- Opaque context structure
**		headerf -- Header field name
**		headerv -- Header field value
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
smfi_addheader(ctx, headerf, headerv)
	SMFICTX *ctx;
	char *headerf;
	char *headerv;
{
	/* do we want to copy the stuff or have a special mi_wr_cmd call? */
	size_t len, l1, l2;
	int r;
	char *buf;
	struct timeval timeout;

	if (headerf == NULL || *headerf == '\0' || headerv == NULL)
		return MI_FAILURE;
	if (!mi_sendok(ctx, SMFIF_ADDHDRS))
		return MI_FAILURE;
	timeout.tv_sec = ctx->ctx_timeout;
	timeout.tv_usec = 0;
	l1 = strlen(headerf);
	l2 = strlen(headerv);
	len = l1 + l2 + 2;
	buf = malloc(len);
	if (buf == NULL)
		return MI_FAILURE;
	(void) memcpy(buf, headerf, l1 + 1);
	(void) memcpy(buf + l1 + 1, headerv, l2 + 1);
	r = mi_wr_cmd(ctx->ctx_sd, &timeout, SMFIR_ADDHEADER, buf, len);
	free(buf);
	return r;
}

/*
**  SMFI_CHGHEADER -- send a changed header to the MTA
**
**	Parameters:
**		ctx -- Opaque context structure
**		headerf -- Header field name
**		hdridx -- Header index value
**		headerv -- Header field value
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
smfi_chgheader(ctx, headerf, hdridx, headerv)
	SMFICTX *ctx;
	char *headerf;
	mi_int32 hdridx;
	char *headerv;
{
	/* do we want to copy the stuff or have a special mi_wr_cmd call? */
	size_t len, l1, l2;
	int r;
	mi_int32 v;
	char *buf;
	struct timeval timeout;

	if (headerf == NULL || *headerf == '\0')
		return MI_FAILURE;
	if (hdridx < 0)
		return MI_FAILURE;
	if (!mi_sendok(ctx, SMFIF_CHGHDRS))
		return MI_FAILURE;
	timeout.tv_sec = ctx->ctx_timeout;
	timeout.tv_usec = 0;
	if (headerv == NULL)
		headerv = "";
	l1 = strlen(headerf);
	l2 = strlen(headerv);
	len = l1 + l2 + 2 + MILTER_LEN_BYTES;
	buf = malloc(len);
	if (buf == NULL)
		return MI_FAILURE;
	v = htonl(hdridx);
	(void) memcpy(&(buf[0]), (void *) &v, MILTER_LEN_BYTES);
	(void) memcpy(buf + MILTER_LEN_BYTES, headerf, l1 + 1);
	(void) memcpy(buf + MILTER_LEN_BYTES + l1 + 1, headerv, l2 + 1);
	r = mi_wr_cmd(ctx->ctx_sd, &timeout, SMFIR_CHGHEADER, buf, len);
	free(buf);
	return r;
}
/*
**  SMFI_ADDRCPT -- send an additional recipient to the MTA
**
**	Parameters:
**		ctx -- Opaque context structure
**		rcpt -- recipient address
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
smfi_addrcpt(ctx, rcpt)
	SMFICTX *ctx;
	char *rcpt;
{
	size_t len;
	struct timeval timeout;

	if (rcpt == NULL || *rcpt == '\0')
		return MI_FAILURE;
	if (!mi_sendok(ctx, SMFIF_ADDRCPT))
		return MI_FAILURE;
	timeout.tv_sec = ctx->ctx_timeout;
	timeout.tv_usec = 0;
	len = strlen(rcpt) + 1;
	return mi_wr_cmd(ctx->ctx_sd, &timeout, SMFIR_ADDRCPT, rcpt, len);
}
/*
**  SMFI_DELRCPT -- send a recipient to be removed to the MTA
**
**	Parameters:
**		ctx -- Opaque context structure
**		rcpt -- recipient address
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
smfi_delrcpt(ctx, rcpt)
	SMFICTX *ctx;
	char *rcpt;
{
	size_t len;
	struct timeval timeout;

	if (rcpt == NULL || *rcpt == '\0')
		return MI_FAILURE;
	if (!mi_sendok(ctx, SMFIF_DELRCPT))
		return MI_FAILURE;
	timeout.tv_sec = ctx->ctx_timeout;
	timeout.tv_usec = 0;
	len = strlen(rcpt) + 1;
	return mi_wr_cmd(ctx->ctx_sd, &timeout, SMFIR_DELRCPT, rcpt, len);
}
/*
**  SMFI_REPLACEBODY -- send a body chunk to the MTA
**
**	Parameters:
**		ctx -- Opaque context structure
**		bodyp -- body chunk
**		bodylen -- length of body chunk
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
smfi_replacebody(ctx, bodyp, bodylen)
	SMFICTX *ctx;
	u_char *bodyp;
	int bodylen;
{
	int len, off, r;
	struct timeval timeout;

	if (bodyp == NULL && bodylen > 0)
		return MI_FAILURE;
	if (!mi_sendok(ctx, SMFIF_CHGBODY))
		return MI_FAILURE;
	timeout.tv_sec = ctx->ctx_timeout;
	timeout.tv_usec = 0;

	/* split body chunk if necessary */
	off = 0;
	while (bodylen > 0)
	{
		len = (bodylen >= MILTER_CHUNK_SIZE) ? MILTER_CHUNK_SIZE :
						       bodylen;
		if ((r = mi_wr_cmd(ctx->ctx_sd, &timeout, SMFIR_REPLBODY,
				(char *) (bodyp + off), len)) != MI_SUCCESS)
			return r;
		off += len;
		bodylen -= len;
	}
	return MI_SUCCESS;
}
/*
**  MYISENHSC -- check whether a string contains an enhanced status code
**
**	Parameters:
**		s -- string with possible enhanced status code.
**		delim -- delim for enhanced status code.
**
**	Returns:
**		0  -- no enhanced status code.
**		>4 -- length of enhanced status code.
**
**	Side Effects:
**		none.
*/
static int
myisenhsc(s, delim)
	const char *s;
	int delim;
{
	int l, h;

	if (s == NULL)
		return 0;
	if (!((*s == '2' || *s == '4' || *s == '5') && s[1] == '.'))
		return 0;
	h = 0;
	l = 2;
	while (h < 3 && isascii(s[l + h]) && isdigit(s[l + h]))
		++h;
	if (h == 0 || s[l + h] != '.')
		return 0;
	l += h + 1;
	h = 0;
	while (h < 3 && isascii(s[l + h]) && isdigit(s[l + h]))
		++h;
	if (h == 0 || s[l + h] != delim)
		return 0;
	return l + h;
}
/*
**  SMFI_SETREPLY -- set the reply code for the next reply to the MTA
**
**	Parameters:
**		ctx -- Opaque context structure
**		rcode -- The three-digit (RFC 821) SMTP reply code.
**		xcode -- The extended (RFC 2034) reply code.
**		message -- The text part of the SMTP reply.
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
smfi_setreply(ctx, rcode, xcode, message)
	SMFICTX *ctx;
	char *rcode;
	char *xcode;
	char *message;
{
	size_t len, l1, l2, l3;
	char *buf;

	if (rcode == NULL || ctx == NULL)
		return MI_FAILURE;
	l1 = strlen(rcode) + 1;
	if (l1 != 4)
		return MI_FAILURE;
	if ((rcode[0] != '4' && rcode[0] != '5') ||
	    !isascii(rcode[1]) || !isdigit(rcode[1]) ||
	    !isascii(rcode[2]) || !isdigit(rcode[2]))
		return MI_FAILURE;
	l2 = xcode == NULL ? 1 : strlen(xcode) + 1;
	if (xcode != NULL && !myisenhsc(xcode, '\0'))
		return MI_FAILURE;
	l3 = message == NULL ? 1 : strlen(message) + 1;
	len = l1 + l2 + l3;
	buf = malloc(len);
	if (buf == NULL)
		return MI_FAILURE;		/* oops */
	(void) snprintf(buf, len, "%s %s %s", rcode,
			xcode == NULL ? "" : xcode,
			message == NULL ? "" : message);
	if (ctx->ctx_reply != NULL)
		free(ctx->ctx_reply);
	ctx->ctx_reply = buf;
	return MI_SUCCESS;
}
/*
**  SMFI_SETPRIV -- set private data
**
**	Parameters:
**		ctx -- Opaque context structure
**		privatedata -- pointer to private data
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

int
smfi_setpriv(ctx, privatedata)
	SMFICTX *ctx;
	void *privatedata;
{
	if (ctx == NULL)
		return MI_FAILURE;
	ctx->ctx_privdata = privatedata;
	return MI_SUCCESS;
}
/*
**  SMFI_GETPRIV -- get private data
**
**	Parameters:
**		ctx -- Opaque context structure
**
**	Returns:
**		pointer to private data
*/

void *
smfi_getpriv(ctx)
	SMFICTX *ctx;
{
	if (ctx == NULL)
		return NULL;
	return ctx->ctx_privdata;
}
/*
**  SMFI_GETSYMVAL -- get the value of a macro
**
**	See explanation in mfapi.h about layout of the structures.
**
**	Parameters:
**		ctx -- Opaque context structure
**		symname -- name of macro
**
**	Returns:
**		value of macro (NULL in case of failure)
*/

char *
smfi_getsymval(ctx, symname)
	SMFICTX *ctx;
	char *symname;
{
	int i;
	char **s;
	char one[2];
	char braces[4];

	if (ctx == NULL || symname == NULL || *symname == '\0')
		return NULL;

	if (strlen(symname) == 3 && symname[0] == '{' && symname[2] == '}')
	{
		one[0] = symname[1];
		one[1] = '\0';
	}
	else
		one[0] = '\0';
	if (strlen(symname) == 1)
	{
		braces[0] = '{';
		braces[1] = *symname;
		braces[2] = '}';
		braces[3] = '\0';
	}
	else
		braces[0] = '\0';

	/* search backwards through the macro array */
	for (i = MAX_MACROS_ENTRIES - 1 ; i >= 0; --i)
	{
		if ((s = ctx->ctx_mac_ptr[i]) == NULL ||
		    ctx->ctx_mac_buf[i] == NULL)
			continue;
		while (s != NULL && *s != NULL)
		{
			if (strcmp(*s, symname) == 0)
				return *++s;
			if (one[0] != '\0' && strcmp(*s, one) == 0)
				return *++s;
			if (braces[0] != '\0' && strcmp(*s, braces) == 0)
				return *++s;
			++s;	/* skip over macro value */
			++s;	/* points to next macro name */
		}
	}
	return NULL;
}
#endif /* _FFR_MILTER */
