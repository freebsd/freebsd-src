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
 */

#ifndef lint
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>

#include "pwupd.h"

int
extendline(char **buf, int * buflen, int needed)
{
	if (needed > *buflen) {
		char	*tmp = realloc(*buf, needed);
		if (tmp == NULL)
			return -1;
		*buf = tmp;
		*buflen = needed;
	}
	return *buflen;
}

int
extendarray(char ***buf, int * buflen, int needed)
{
	if (needed > *buflen) {
		char	**tmp = realloc(*buf, needed * sizeof(char *));
		if (tmp == NULL)
			return -1;
		*buf = tmp;
		*buflen = needed;
	}
	return *buflen;
}


int
fileupdate(char const * filename, mode_t fmode, char const * newline, char const * prefix, int pfxlen, int updmode)
{
	int             rc = 0;

	if (pfxlen <= 1)
		errno = EINVAL;
	else {
		int             infd = open(filename, O_RDWR | O_CREAT | O_EXLOCK, fmode);

		if (infd != -1) {
			FILE           *infp = fdopen(infd, "r+");

			if (infp == NULL)
				close(infd);
			else {
				int             outfd;
				char            file[MAXPATHLEN];

				strcpy(file, filename);
				strcat(file, ".new");
				outfd = open(file, O_RDWR | O_CREAT | O_TRUNC | O_EXLOCK, fmode);
				if (outfd != -1) {
					FILE           *outfp = fdopen(outfd, "w+");

					if (outfp == NULL)
						close(outfd);
					else {
						int             updated = UPD_CREATE;
						int		linesize = PWBUFSZ;
						char           *line = malloc(linesize);

					nextline:
						while (fgets(line, linesize, infp) != NULL) {
							char           *p = strchr(line, '\n');

							while ((p = strchr(line, '\n')) == NULL) {
								int	l;
								if (extendline(&line, &linesize, linesize + PWBUFSZ) == -1) {
									int	ch;
									fputs(line, outfp);
									while ((ch = fgetc(infp)) != EOF) {
										fputc(ch, outfp);
										if (ch == '\n')
											break;
									}
									goto nextline;
								}
								l = strlen(line);
								if (fgets(line + l, linesize - l, infp) == NULL)
									break;
							}
							if (*line != '#' && *line != '\n') {
								if (!updated && strncmp(line, prefix, pfxlen) == 0) {
									updated = updmode == UPD_REPLACE ? UPD_REPLACE : UPD_DELETE;

									/*
									 * Only actually write changes if updating
									 */
									if (updmode == UPD_REPLACE)
										strcpy(line, newline);
									else if (updmode == UPD_DELETE)
										continue;
								}
							}
							fputs(line, outfp);
						}

						/*
						 * Now, we need to decide what to do: If we are in
						 * update mode, and no record was updated, then error If
						 * we are in insert mode, and record already exists,
						 * then error
						 */
						if (updmode != updated)
							errno = (updmode == UPD_CREATE) ? EEXIST : ENOENT;
						else {

							/*
							 * If adding a new record, append it to the end
							 */
							if (updmode == UPD_CREATE)
								fputs(newline, outfp);

							/*
							 * Flush the file and check for the result
							 */
							rc = fflush(outfp) != EOF;
							if (rc) {

								/*
								 * Copy data back into the
								 * original file and truncate
								 */
								rewind(infp);
								rewind(outfp);
								while (fgets(line, linesize, outfp) != NULL)
									fputs(line, infp);

								/*
								 * This is a gross hack, but we may have
								 * corrupted the original file
								 * Unfortunately, it will lose the inode.
								 */
								if (fflush(infp) == EOF || ferror(infp))
									rc = rename(file, filename) == 0;
								else
									ftruncate(infd, ftell(infp));
							}
						}
						free(line);
						fclose(outfp);
					}
					remove(file);
				}
				fclose(infp);
			}
		}
	}
	return rc;
}
