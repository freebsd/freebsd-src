/*
 *  Copyright (c) 1999-2003 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: comm.c,v 8.54.2.6 2003/01/03 22:14:40 ca Exp $")

#include "libmilter.h"
#include <sm/errstring.h>

/*
**  MI_RD_CMD -- read a command
**
**	Parameters:
**		sd -- socket descriptor
**		timeout -- maximum time to wait
**		cmd -- single character command read from sd
**		rlen -- pointer to length of result
**		name -- name of milter
**
**	Returns:
**		buffer with rest of command
**		(malloc()ed here, should be free()d)
**		hack: encode error in cmd
*/

char *
mi_rd_cmd(sd, timeout, cmd, rlen, name)
	socket_t sd;
	struct timeval *timeout;
	char *cmd;
	size_t *rlen;
	char *name;
{
	ssize_t len;
	mi_int32 expl;
	ssize_t i;
	FD_RD_VAR(rds, excs);
	int ret;
	int save_errno;
	char *buf;
	char data[MILTER_LEN_BYTES + 1];

	*cmd = '\0';
	*rlen = 0;

	i = 0;
	for (;;)
	{
		FD_RD_INIT(sd, rds, excs);
		ret = FD_RD_READY(sd, rds, excs, timeout);
		if (ret == 0)
			break;
		else if (ret < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		if (FD_IS_RD_EXC(sd, rds, excs))
		{
			*cmd = SMFIC_SELECT;
			return NULL;
		}

		len = MI_SOCK_READ(sd, data + i, sizeof data - i);
		if (MI_SOCK_READ_FAIL(len))
		{
			smi_log(SMI_LOG_ERR,
				"%s, mi_rd_cmd: read returned %d: %s",
				name, (int) len, sm_errstring(errno));
			*cmd = SMFIC_RECVERR;
			return NULL;
		}
		if (len == 0)
		{
			*cmd = SMFIC_EOF;
			return NULL;
		}
		if (len >= (ssize_t) sizeof data - i)
			break;
		i += len;
	}
	if (ret == 0)
	{
		*cmd = SMFIC_TIMEOUT;
		return NULL;
	}
	else if (ret < 0)
	{
		smi_log(SMI_LOG_ERR,
			"%s: mi_rd_cmd: select returned %d: %s",
			name, ret, sm_errstring(errno));
		*cmd = SMFIC_RECVERR;
		return NULL;
	}

	*cmd = data[MILTER_LEN_BYTES];
	data[MILTER_LEN_BYTES] = '\0';
	(void) memcpy((void *) &expl, (void *) &(data[0]), MILTER_LEN_BYTES);
	expl = ntohl(expl) - 1;
	if (expl <= 0)
		return NULL;
	if (expl > MILTER_CHUNK_SIZE)
	{
		*cmd = SMFIC_TOOBIG;
		return NULL;
	}
#if _FFR_ADD_NULL
	buf = malloc(expl + 1);
#else /* _FFR_ADD_NULL */
	buf = malloc(expl);
#endif /* _FFR_ADD_NULL */
	if (buf == NULL)
	{
		*cmd = SMFIC_MALLOC;
		return NULL;
	}

	i = 0;
	for (;;)
	{
		FD_RD_INIT(sd, rds, excs);
		ret = FD_RD_READY(sd, rds, excs, timeout);
		if (ret == 0)
			break;
		else if (ret < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		if (FD_IS_RD_EXC(sd, rds, excs))
		{
			*cmd = SMFIC_SELECT;
			free(buf);
			return NULL;
		}
		len = MI_SOCK_READ(sd, buf + i, expl - i);
		if (MI_SOCK_READ_FAIL(len))
		{
			smi_log(SMI_LOG_ERR,
				"%s: mi_rd_cmd: read returned %d: %s",
				name, (int) len, sm_errstring(errno));
			ret = -1;
			break;
		}
		if (len == 0)
		{
			*cmd = SMFIC_EOF;
			free(buf);
			return NULL;
		}
		if (len > expl - i)
		{
			*cmd = SMFIC_RECVERR;
			free(buf);
			return NULL;
		}
		if (len >= expl - i)
		{
			*rlen = expl;
#if _FFR_ADD_NULL
			/* makes life simpler for common string routines */
			buf[expl] = '\0';
#endif /* _FFR_ADD_NULL */
			return buf;
		}
		i += len;
	}

	save_errno = errno;
	free(buf);

	/* select returned 0 (timeout) or < 0 (error) */
	if (ret == 0)
	{
		*cmd = SMFIC_TIMEOUT;
		return NULL;
	}
	if (ret < 0)
	{
		smi_log(SMI_LOG_ERR,
			"%s: mi_rd_cmd: select returned %d: %s",
			name, ret, sm_errstring(save_errno));
		*cmd = SMFIC_RECVERR;
		return NULL;
	}
	*cmd = SMFIC_UNKNERR;
	return NULL;
}
/*
**  MI_WR_CMD -- write a cmd to sd
**
**	Parameters:
**		sd -- socket descriptor
**		timeout -- maximum time to wait (currently unused)
**		cmd -- single character command to write
**		buf -- buffer with further data
**		len -- length of buffer (without cmd!)
**
**	Returns:
**		MI_SUCCESS/MI_FAILURE
*/

/*
**  we don't care much about the timeout here, it's very long anyway
**  FD_SETSIZE is checked when socket is created.
**  XXX l == 0 ?
*/

#define MI_WR(data)	\
	while (sl > 0)							\
	{								\
		FD_WR_INIT(sd, wrs);					\
		ret = FD_WR_READY(sd, wrs, timeout);			\
		if (ret == 0)						\
			return MI_FAILURE;				\
		if (ret < 0)						\
		{							\
			if (errno == EINTR)				\
				continue;				\
			else						\
				return MI_FAILURE;			\
		}							\
		l = MI_SOCK_WRITE(sd, (void *) ((data) + i), sl);	\
		if (l < 0)						\
		{							\
			if (errno == EINTR)				\
				continue;				\
			else						\
				return MI_FAILURE;			\
		}							\
		i += l;							\
		sl -= l;						\
	}

int
mi_wr_cmd(sd, timeout, cmd, buf, len)
	socket_t sd;
	struct timeval *timeout;
	int cmd;
	char *buf;
	size_t len;
{
	size_t sl, i;
	ssize_t l;
	mi_int32 nl;
	int ret;
	FD_WR_VAR(wrs);
	char data[MILTER_LEN_BYTES + 1];

	if (len > MILTER_CHUNK_SIZE)
		return MI_FAILURE;
	nl = htonl(len + 1);	/* add 1 for the cmd char */
	(void) memcpy(data, (void *) &nl, MILTER_LEN_BYTES);
	data[MILTER_LEN_BYTES] = (char) cmd;
	i = 0;
	sl = MILTER_LEN_BYTES + 1;

	/* use writev() instead to send the whole stuff at once? */

	MI_WR(data);
	if (len > 0 && buf == NULL)
		return MI_FAILURE;
	if (len == 0 || buf == NULL)
		return MI_SUCCESS;
	i = 0;
	sl = len;
	MI_WR(buf);
	return MI_SUCCESS;
}
