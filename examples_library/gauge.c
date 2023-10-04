/*-
 * SPDX-License-Identifier: CC0-1.0
 *
 * Written in 2023 by Alfonso Sabato Siciliano.
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty, see:
 *   <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <bsddialog.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void sender(int fd)
{
	int i;

	for (i = 1; i <= 10; i++) {
		sleep(1);
		dprintf(fd, "SEP\n");
		dprintf(fd, "%d\n", i * 10);
		dprintf(fd, "In Progress... [%d / 10]\n", i);
		dprintf(fd, "SEP\n");
	}
	sleep(1);
	dprintf(fd, "EOF\n");
}

int main()
{
	int rv, fd[2];
	struct bsddialog_conf conf;

	/* add checks and sync */
	pipe(fd);
	if (fork() == 0) {
		close(fd[0]);
		sender(fd[1]);
		exit (0);
	}
	close(fd[1]);

	if (bsddialog_init() == BSDDIALOG_ERROR) {
		printf("Error: %s\n", bsddialog_geterror());
		return (1);
	}
	bsddialog_initconf(&conf);
	conf.title = "gauge";
	rv = bsddialog_gauge(&conf, "Example", 7, 30, 0, fd[0], "SEP", "EOF");
	bsddialog_end();
	if(rv == BSDDIALOG_ERROR)
		printf("Error: %s\n", bsddialog_geterror());

	return (0);
}