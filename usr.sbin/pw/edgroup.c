/*-
 * Copyright (c) 1996 by David L. Nugent <davidn@blaze.net.au>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by David L. Nugent.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DAVID L. NUGENT ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <sys/param.h>
#include <ctype.h>

#include "pwupd.h"

static int
isingroup(char const * name, char **mem)
{
	int             i;

	for (i = 0; i < MAXGROUPS && mem[i] != NULL; i++)
		if (strcmp(name, mem[i]) == 0)
			return i;
	return -1;
}

static char     groupfile[] = _PATH_GROUP;
static char     grouptmp[] = _PATH_GROUP ".new";

int
editgroups(char *name, char **groups)
{
	int             rc = 0;
	int             infd;

	if ((infd = open(groupfile, O_RDWR | O_CREAT | O_EXLOCK, 0644)) != -1) {
		FILE           *infp;

		if ((infp = fdopen(infd, "r+")) == NULL)
			close(infd);
		else {
			int             outfd;

			if ((outfd = open(grouptmp, O_RDWR | O_CREAT | O_TRUNC | O_EXLOCK, 0644)) != -1) {
				FILE           *outfp;

				if ((outfp = fdopen(outfd, "w+")) == NULL)
					close(outfd);
				else {
					char            line[MAXPWLINE];
					char            outl[MAXPWLINE];

					while (fgets(line, sizeof(line), infp) != NULL) {
						char           *p = strchr(line, '\n');

						if (p == NULL) {	/* Line too long */
							int             ch;

							fputs(line, outfp);
							while ((ch = fgetc(infp)) != EOF) {
								fputc(ch, outfp);
								if (ch == '\n')
									break;
							}
							continue;
						}
						if (*line == '#')
							strcpy(outl, line);
						else if (*line == '\n')
							*outl = '\0';
						else {
							int             i,
							                mno = 0;
							char           *cp = line;
							char const     *sep = ":\n";
							struct group    grp;
							char           *mems[MAXGROUPS];

							memset(&grp, 0, sizeof grp);
							grp.gr_mem = mems;
							for (i = 0; (p = strsep(&cp, sep)) != NULL; i++) {
								switch (i) {
								case 0:	/* Group name */
									grp.gr_name = p;
									break;
								case 1:	/* Group password */
									grp.gr_passwd = p;
									break;
								case 2:	/* Group id */
									grp.gr_gid = atoi(p);
									break;
								case 3:	/* Member list */
									cp = p;
									sep = ",\n";
									break;
								default:	/* Individual members */
									if (mno < MAXGROUPS && *p)
										mems[mno++] = p;
									break;
								}
							}
							if (i < 2)	/* Bail out -
									 * insufficient fields */
								continue;

							for (i = mno; i < MAXGROUPS; i++)
								mems[i] = NULL;

							/*
							 * Delete from group, or add to group?
							 */
							if (groups == NULL || isingroup(grp.gr_name, groups) == -1) {	/* Delete */
								int             idx;

								while ((idx = isingroup(name, mems)) != -1) {
									for (i = idx; i < (MAXGROUPS - 1); i++)
										mems[i] = mems[i + 1];
									mems[i] = NULL;
									--mno;
								}

								/*
								 * Special case - deleting user and group may be user's own
								 */
								if (groups == NULL && mems[0] == NULL && strcmp(name, grp.gr_name) == 0) {	/* First, make _sure_ we
																		 * don't have other
																		 * members */
									struct passwd  *pwd;

									setpwent();
									while ((pwd = getpwent()) != NULL && pwd->pw_gid != grp.gr_gid);
									endpwent();
									if (pwd == NULL)	/* No members at all */
										continue;	/* Drop the group */
								}
							} else if (isingroup(name, mems) == -1)
								mems[mno++] = name;
							fmtgrentry(outl, &grp, PWF_GROUP);
						}
						fputs(outl, outfp);
					}
					if (fflush(outfp) != EOF) {
						rc = 1;

						/*
						 * Copy data back into the original file and truncate
						 */
						rewind(infp);
						rewind(outfp);
						while (fgets(line, sizeof(line), outfp) != NULL)
							fputs(line, infp);

						/*
						 * This is a gross hack, but we may have corrupted the
						 * original file. Unfortunately, it will lose preservation
						 * of the inode.
						 */
						if (fflush(infp) == EOF || ferror(infp))
							rc = rename(grouptmp, groupfile) == 0;
						else
							ftruncate(infd, ftell(infp));
					}
					fclose(outfp);
				}
				remove(grouptmp);
			}
			fclose(infp);
		}
	}
	return rc;
}
