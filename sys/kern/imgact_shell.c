/*
 * Copyright (c) 1993, David Greenman
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kernel.h>

#if BYTE_ORDER == LITTLE_ENDIAN
#define SHELLMAGIC	0x2123 /* #! */
#else
#define SHELLMAGIC	0x2321
#endif

/*
 * Shell interpreter image activator. A interpreter name beginning
 *	at imgp->stringbase is the minimal successful exit requirement.
 */
int
exec_shell_imgact(imgp)
	struct image_params *imgp;
{
	const char *image_header = imgp->image_header;
	const char *ihp, *line_endp;
	char *interp;

	/* a shell script? */
	if (((const short *) image_header)[0] != SHELLMAGIC)
		return(-1);

	/*
	 * Don't allow a shell script to be the shell for a shell
	 *	script. :-)
	 */
	if (imgp->interpreted)
		return(ENOEXEC);

	imgp->interpreted = 1;

	/*
	 * Copy shell name and arguments from image_header into string
	 *	buffer.
	 */

	/*
	 * Find end of line; return if the line > MAXSHELLCMDLEN long.
	 */
	for (ihp = &image_header[2]; *ihp != '\n' && *ihp != '#'; ++ihp) {
		if (ihp >= &image_header[MAXSHELLCMDLEN])
			return(ENOEXEC);
	}
	line_endp = ihp;

	/* reset for another pass */
	ihp = &image_header[2];

	/* Skip over leading spaces - until the interpreter name */
	while ((*ihp == ' ') || (*ihp == '\t')) ihp++;

	/* copy the interpreter name */
	interp = imgp->interpreter_name;
	while ((ihp < line_endp) && (*ihp != ' ') && (*ihp != '\t'))
		*interp++ = *ihp++;
	*interp = '\0';

	/* Disallow a null interpreter filename */
	if (*imgp->interpreter_name == '\0')
		return(ENOEXEC);

	/* reset for another pass */
	ihp = &image_header[2];

	/* copy the interpreter name and arguments */
	while (ihp < line_endp) {
		/* Skip over leading spaces */
		while ((*ihp == ' ') || (*ihp == '\t')) ihp++;

		if (ihp < line_endp) {
			/*
			 * Copy to end of token. No need to watch stringspace
			 *	because this is at the front of the string buffer
			 *	and the maximum shell command length is tiny.
			 */
			while ((ihp < line_endp) && (*ihp != ' ') && (*ihp != '\t')) {
				*imgp->stringp++ = *ihp++;
				imgp->stringspace--;
			}

			*imgp->stringp++ = 0;
			imgp->stringspace--;

			imgp->argc++;
		}
	}

	imgp->argv0 = imgp->uap->fname;

	return(0);
}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 */
static struct execsw shell_execsw = { exec_shell_imgact, "#!" };
EXEC_SET(shell, shell_execsw);
