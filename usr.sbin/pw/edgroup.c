/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
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

	for (i = 0; mem[i] != NULL; i++)
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
					int		linelen = PWBUFSZ;
					int		outlen =  PWBUFSZ;
					int		memlen = 200; /* Arbitrary */
					char           *line = malloc(linelen);
					char           *outl = malloc(outlen);
					char	      **mems = malloc(memlen * sizeof(char *));
					int		namlen = strlen(name);

					if (line == NULL || outl == NULL || mems == NULL) {
					    mem_abort:
						rc = 0;
					} else {
						while (fgets(line, linelen, infp) != NULL) {
							char           *p;
							int		l;

							while ((p = strchr(line, '\n')) == NULL)
							{
								if (extendline(&line, &linelen, linelen + PWBUFSZ) == -1) {
									goto mem_abort;
								}
								l = strlen(line);
								if (fgets(line + l, linelen - l, infp) == NULL)
									break;	/* No newline terminator on last line */
							}
							l = strlen(line) + namlen + 1;
							if (extendline(&outl, &outlen, l) == -1) {
								goto mem_abort;
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

								memset(&grp, 0, sizeof grp);
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
										if (*p) {
											if (extendarray(&mems, &memlen, mno + 2) == -1) {
												goto mem_abort;
											}
											mems[mno++] = p;
										}
										break;
									}
								}
								if (i < 2)	/* Bail out - insufficient fields */
									continue;

								grp.gr_mem = mems;
								for (i = mno; i < memlen; i++)
									mems[i] = NULL;

								/*
								 * Delete from group, or add to group?
								 */
								if (groups == NULL || isingroup(grp.gr_name, groups) == -1) {	/* Delete */
									int             idx;

									while ((idx = isingroup(name, mems)) != -1) {
										for (i = idx; i < (memlen - 1); i++)
											mems[i] = mems[i + 1];
										mems[i] = NULL;
										--mno;
									}
									/*
									 * Special case - deleting user and group may be user's own
									 */
									if (groups == NULL && mems[0] == NULL && strcmp(name, grp.gr_name) == 0) {
										/*
										 * First, make _sure_ we don't have other members
										 */
										struct passwd  *pwd;

										setpwent();
										while ((pwd = getpwent()) != NULL && pwd->pw_gid != grp.gr_gid);
										endpwent();
										if (pwd == NULL)	/* No members at all */
											continue;	/* Drop the group */
									}
								} else if (isingroup(name, mems) == -1) {
									if (extendarray(&mems, &memlen, mno + 2) == -1) {
										goto mem_abort;
									}
									grp.gr_mem = mems;    /* May have realloced() */
									mems[mno++] = name;
									mems[mno  ] = NULL;
								}
								fmtgrentry(&outl, &outlen, &grp, PWF_GROUP);
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
							while (fgets(outl, outlen, outfp) != NULL)
								fputs(outl, infp);

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
					}
					free(mems);
					free(outl);
			    		free(line);
					fclose(outfp);
				}
				remove(grouptmp);
			}
			fclose(infp);
		}
	}
	return rc;
}
