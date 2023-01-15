/*
 * Copyright (c) 2016 Proofpoint, Inc. and its suppliers.
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

/*
**  NOTIFY_WR -- test of notify feature
**
**	Parameters:
**		pid -- pid of process
**
**	Returns:
**		0 on success
**		< 0 on failure
*/

static int
notify_wr(pid)
	pid_t pid;
{
	int r;
	size_t len;
	char buf[64];
#define TSTSTR "qf0001"

	r = sm_notify_start(false, 0);
	if (r < 0)
	{
		perror("sm_notify_start failed");
		return -1;
	}

	len = sm_snprintf(buf, sizeof(buf), "%s-%ld", TSTSTR, (long) pid);
	r = sm_notify_snd(buf, len);
	SM_TEST(r >= 0);
	return r;
}

/*
**  NOTIFY_RD -- test of notify feature
**
**	Parameters:
**
**	Returns:
**		0 on success
**		< 0 on failure
*/

static int
notify_rd(nproc)
	int nproc;
{
	int r, i;
	char buf[64];
#define TSTSTR "qf0001"

	r = sm_notify_start(true, 0);
	if (r < 0)
	{
		perror("sm_notify_start failed");
		return -1;
	}

	for (i = 0; i < nproc; i++)
	{
		r = sm_notify_rcv(buf, sizeof(buf), 5 * SM_MICROS);
		SM_TEST(r >= 0);
		if (r < 0)
		{
			fprintf(stderr, "rcv=%d\n", r);
			return r;
		}
		if (r > 0 && r < sizeof(buf))
			buf[r] = '\0';
		buf[sizeof(buf) - 1] = '\0';
		SM_TEST(strncmp(buf, TSTSTR, sizeof(TSTSTR) - 1) == 0);
		SM_TEST(r > sizeof(TSTSTR));
		fprintf(stderr, "buf=\"%s\"\n", buf);
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
	pid_t pid;

# define OPTIONS	"p:"
	while ((i = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch ((char) i)
		{
		  case 'p':
			nproc = atoi(optarg);
			if (nproc < 1)
			{
				errno = EINVAL;
				perror("-p: must be >0\n");
				return r;
			}
			break;
		  default:
			break;
		}
	}

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
			r = notify_wr(getpid());
			break;
		}
	}
	if (pid > 0)
		r = notify_rd(nproc);
	SM_TEST(r >= 0);
	return sm_test_end();
}
#else /* _FFR_DMTRIGGER */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	printf("SKIPPED: no _FFR_DMTRIGGER\n");
	return 0;
}
#endif /* _FFR_DMTRIGGER */
