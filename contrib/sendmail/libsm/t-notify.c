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

# include <stdlib.h>
# include <unistd.h>

# include <sm/heap.h>
# include <sm/string.h>
# include <sm/test.h>
# include <sm/notify.h>

# define MAX_CNT	10

/*
**  MSGTEST -- test of message queue.
**
**	Parameters:
**		owner -- create message queue.
**
**	Returns:
**		0 on success
**		< 0 on failure.
*/

static int
notifytest(owner)
	int owner;
{
	int r;
	size_t len;
	char buf[64];
#define TSTSTR "qf0001"

	r = sm_notify_start(owner, 0);
	if (r < 0)
	{
		perror("sm_notify_start failed");
		return -1;
	}

	if (!owner)
	{
		len = sm_strlcpy(buf, TSTSTR, sizeof(buf));
		r = sm_notify_snd(buf, len);
		SM_TEST(r >= 0);
		if (r < 0)
			goto end;

  end:
		return r;
	}
	else
	{
		r = sm_notify_rcv(buf, sizeof(buf), 5);
		SM_TEST(r >= 0);
		if (r < 0)
			return r;
		if (r > 0 && r < sizeof(buf))
			buf[r] = '\0';
		buf[sizeof(buf) - 1] = '\0';
		SM_TEST(strcmp(buf, TSTSTR) == 0);
		fprintf(stderr, "buf=\"%s\"\n", buf);
	}
	return 0;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	int r = 0;
	pid_t pid;

# define OPTIONS	""
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch ((char) ch)
		{
		  default:
			break;
		}
	}

	r = sm_notify_init(0);
	if (r < 0)
	{
		perror("sm_notify_init failed\n");
		return -1;
	}

	if ((pid = fork()) < 0)
	{
		perror("fork failed\n");
		return -1;
	}

	sm_test_begin(argc, argv, "test notify");
	if (pid == 0)
	{
		/* give the parent the chance to setup data */
		sleep(1);
		r = notifytest(false);
	}
	else
	{
		r = notifytest(true);
	}
	SM_TEST(r >= 0);
	return sm_test_end();
}
