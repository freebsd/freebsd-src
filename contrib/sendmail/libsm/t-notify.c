/*
 * Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#include <sm/gen.h>
#include <stdio.h>

#if _FFR_DMTRIGGER || _FFR_NOTIFY
# include <stdlib.h>
# include <unistd.h>
# include <errno.h>
# include <sm/heap.h>
# include <sm/string.h>
# include <sm/test.h>
# include <sm/notify.h>
# include <sm/conf.h>
# include "notify.h"

static int Verbose = 0;
#define MAX_CHILDREN	256
#define MAX_MSGS	1024
static pid_t pids[MAX_CHILDREN];
static char msgs[MAX_CHILDREN][MAX_MSGS];

/*
**  NOTIFY_WR -- test of notify write feature
**
**	Parameters:
**		pid -- pid of process
**		nmsgs -- number of messages to write
**
**	Returns:
**		>=0 on success
**		< 0 on failure
*/

static int
notify_wr(pid, nmsgs)
	pid_t pid;
	int nmsgs;
{
	int r, i;
	size_t len;
	char buf[64];
#define TSTSTR "qf0001"

	r = sm_notify_start(false, 0);
	if (r < 0)
	{
		perror("sm_notify_start failed");
		return -1;
	}

	for (i = 0; i < nmsgs; i++)
	{
		len = sm_snprintf(buf, sizeof(buf), "%s-%ld_%d", TSTSTR,
				(long) pid, i);
		r = sm_notify_snd(buf, len);
		SM_TEST(r >= 0);
	}
	return r;
}

static int
validpid(nproc, cpid)
	int nproc;
	pid_t cpid;
{
	int i;

	for (i = 0; i < nproc; i++)
		if (cpid == pids[i])
			return i;
	if (Verbose > 0)
		fprintf(stderr, "pid=%ld not found, nproc=%d\n",
			(long) cpid, nproc);
	return -1;
}

/*
**  NOTIFY_RD -- test of notify read feature
**
**	Parameters:
**		nproc -- number of processes started
**		nmsgs -- number of messages to read for each process
**
**	Returns:
**		0 on success
**		< 0 on failure
*/

static int
notify_rd(nproc, nmsgs)
	int nproc;
	int nmsgs;
{
	int r, i, pidx;
	long cpid;
	char buf[64], *p;
#define TSTSTR "qf0001"

	r = sm_notify_start(true, 0);
	if (r < 0)
	{
		perror("sm_notify_start failed");
		return -1;
	}

	for (i = 0; i < nmsgs * nproc; i++)
	{
		do
		{
			r = sm_notify_rcv(buf, sizeof(buf), 5 * SM_MICROS);
			SM_TEST(r >= 0);
		} while (0 == r);
		if (r < 0)
		{
			fprintf(stderr, "pid=%ld, rcv=%d, i=%d\n",
				(long)getpid(), r, i);
			return r;
		}
		if (r > 0 && r < sizeof(buf))
			buf[r] = '\0';
		buf[sizeof(buf) - 1] = '\0';

		if (Verbose > 0)
			fprintf(stderr, "pid=%ld, buf=\"%s\", i=%d\n",
				(long)getpid(), buf, i);

		SM_TEST(strncmp(buf, TSTSTR, sizeof(TSTSTR) - 1) == 0);
		SM_TEST(r > sizeof(TSTSTR));

		r = sscanf(buf + sizeof(TSTSTR), "%ld", &cpid);
		SM_TEST(1 == r);
		pidx = validpid(nproc, (pid_t)cpid);
		SM_TEST(pidx >= 0);
		SM_TEST(pidx < nproc);

		p = strchr(buf, '_');
		SM_TEST(NULL != p);
		if (NULL != p && pidx < nproc && pidx >= 0)
		{
			int n;

			r = sscanf(p + 1, "%d", &n);
			SM_TEST(1 == r);
			SM_TEST(n >= 0);
			SM_TEST(n < nmsgs);
			if (1 == r && n < nmsgs && n >= 0)
			{
				SM_TEST('\0' == msgs[pidx][n]);
				msgs[pidx][n] = 'f';
			}
		}
	}
	return 0;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int i;
	int r = 0;
	int nproc = 1;
	int nmsgs = 1;
	pid_t pid;

# define OPTIONS	"n:p:V"
	while ((i = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch ((char) i)
		{
		  case 'n':
			nmsgs = atoi(optarg);
			if (nmsgs < 1)
			{
				errno = EINVAL;
				fprintf(stderr, "-%c: must be >0\n", (char) i);
				return 1;
			}
			if (nmsgs >= MAX_MSGS)
			{
				errno = EINVAL;
				fprintf(stderr, "-%c: must be <%d\n", (char) i, MAX_MSGS);
				return 1;
			}
			break;
		  case 'p':
			nproc = atoi(optarg);
			if (nproc < 1)
			{
				errno = EINVAL;
				fprintf(stderr, "-%c: must be >0\n", (char) i);
				return 1;
			}
			if (nproc >= MAX_CHILDREN)
			{
				errno = EINVAL;
				fprintf(stderr, "-%c: must be <%d\n", (char) i, MAX_CHILDREN);
				return 1;
			}
			break;
		  case 'V':
			++Verbose;
			break;
		  default:
			break;
		}
	}

	memset(msgs, '\0', sizeof(msgs));
	sm_test_begin(argc, argv, "test notify");
	r = sm_notify_init(0);
	SM_TEST(r >= 0);
	if (r < 0)
	{
		perror("sm_notify_init failed\n");
		return r;
	}

	pid = 0;
	for (i = 0; i < nproc; i++)
	{
		if ((pid = fork()) < 0)
		{
			perror("fork failed\n");
			return -1;
		}

		if (pid == 0)
		{
			/* give the parent the chance to set up data */
			sleep(1);
			r = notify_wr(getpid(), nmsgs);
			break;
		}
		if (pid > 0)
			pids[i] = pid;
	}
	if (pid > 0)
		r = notify_rd(nproc, nmsgs);
	SM_TEST(r >= 0);
	return sm_test_end();
}
#else /* _FFR_DMTRIGGER */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	printf("SKIPPED: no _FFR_DMTRIGGER || _FFR_NOTIFY\n");
	return 0;
}
#endif /* _FFR_DMTRIGGER || _FFR_NOTIFY */
