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
static char id[] = "@(#)$Id: comm.c,v 8.30.4.6 2000/10/05 22:44:01 gshapiro Exp $";
#endif /* ! lint */

#if _FFR_MILTER
#include "libmilter.h"

#define FD_Z	FD_ZERO(&readset);	\
		FD_SET((u_int) sd, &readset);	\
		FD_ZERO(&excset);	\
		FD_SET((u_int) sd, &excset)

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
	fd_set readset, excset;
	int ret;
	int save_errno;
	char *buf;
	char data[MILTER_LEN_BYTES + 1];

	*cmd = '\0';
	*rlen = 0;

	if (sd >= FD_SETSIZE)
	{
		smi_log(SMI_LOG_ERR, "%s: fd %d is larger than FD_SETSIZE %d",
			name, sd, FD_SETSIZE);
		*cmd = SMFIC_SELECT;
		return NULL;
	}

	FD_Z;
	i = 0;
	while ((ret = select(sd + 1, &readset, NULL, &excset, timeout)) >= 1)
	{
		if (FD_ISSET(sd, &excset))
		{
			*cmd = SMFIC_SELECT;
			return NULL;
		}
		if ((len = MI_SOCK_READ(sd, data + i, sizeof data - i)) < 0)
		{
			smi_log(SMI_LOG_ERR,
				"%s, mi_rd_cmd: read returned %d: %s",
				name, len, strerror(errno));
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
		FD_Z;
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
			name, ret, strerror(errno));
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
	buf = malloc(expl);
	if (buf == NULL)
	{
		*cmd = SMFIC_MALLOC;
		return NULL;
	}

	i = 0;
	FD_Z;
	while ((ret = select(sd + 1, &readset, NULL, &excset, timeout)) == 1)
	{
		if (FD_ISSET(sd, &excset))
		{
			*cmd = SMFIC_SELECT;
			free(buf);
			return NULL;
		}
		if ((len = MI_SOCK_READ(sd, buf + i, expl - i)) < 0)
		{
			smi_log(SMI_LOG_ERR,
				"%s: mi_rd_cmd: read returned %d: %s",
				name, len, strerror(errno));
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
			return buf;
		}
		i += len;
		FD_Z;
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
			name, ret, strerror(save_errno));
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
	fd_set wrtset;
	char data[MILTER_LEN_BYTES + 1];

	if (len > MILTER_CHUNK_SIZE)
		return MI_FAILURE;
	nl = htonl(len + 1);	/* add 1 for the cmd char */
	(void) memcpy(data, (void *) &nl, MILTER_LEN_BYTES);
	data[MILTER_LEN_BYTES] = (char) cmd;
	i = 0;
	sl = MILTER_LEN_BYTES + 1;

	do
	{
		FD_ZERO(&wrtset);
		FD_SET((u_int) sd, &wrtset);
		if ((ret = select(sd + 1, NULL, &wrtset, NULL, timeout)) == 0)
			return MI_FAILURE;
	} while (ret < 0 && errno == EINTR);
	if (ret < 0)
		return MI_FAILURE;

	/* use writev() instead to send the whole stuff at once? */
	while ((l = MI_SOCK_WRITE(sd, (void *) (data + i),
				  sl - i)) < (ssize_t) sl)
	{
		if (l < 0)
			return MI_FAILURE;
		i += l;
		sl -= l;
	}

	if (len > 0 && buf == NULL)
		return MI_FAILURE;
	if (len == 0 || buf == NULL)
		return MI_SUCCESS;
	i = 0;
	sl = len;
	do
	{
		FD_ZERO(&wrtset);
		FD_SET((u_int) sd, &wrtset);
		if ((ret = select(sd + 1, NULL, &wrtset, NULL, timeout)) == 0)
			return MI_FAILURE;
	} while (ret < 0 && errno == EINTR);
	if (ret < 0)
		return MI_FAILURE;
	while ((l = MI_SOCK_WRITE(sd, (void *) (buf + i),
				  sl - i)) < (ssize_t) sl)
	{
		if (l < 0)
			return MI_FAILURE;
		i += l;
		sl -= l;
	}
	return MI_SUCCESS;
}
#endif /* _FFR_MILTER */
