/*
 * Program:	wargames(6)
 * Author:	Juli Mallett <jmallett@FreeBSD.org>
 * Copyright:	This file is in the public domain.
 * Description:
 * 	Would you like to play a game?  Or is the game you chose just a practice
 * 	in futility...  Based on the original Berkeley shell script, inspired by
 * 	the motion picture.
 *
 * From:	@(#)wargames.sh 8.1 (Berkeley) 5/31/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
	struct stat sb;
	char buffer[MAXPATHLEN];
	char *line;
	size_t len;

	printf("Would you like to play a game? ");
	line = fgetln(stdin, &len);
	if (line == NULL) {
		err(1, "I'm sorry to hear that");
	}
	line[len - 1] = '\0';
	snprintf(buffer, sizeof buffer, "/usr/games/%s", line);
	if (stat(buffer, &sb) != -1) {
		initscr();
		clear();
		endwin();
		execl(buffer, line, NULL);
	}
	printf("Funny, the only way to win is not to play at all.\n");
	return 0;
}
