/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/proc.h>
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
 * Shell interpreter image activator. An interpreter name beginning
 *	at imgp->args->begin_argv is the minimal successful exit requirement.
 */
int
exec_shell_imgact(imgp)
	struct image_params *imgp;
{
	const char *image_header = imgp->image_header;
	const char *ihp;
	int error, offset;
	size_t length, clength;
	struct vattr vattr;

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
	 * At this point we have the first page of the file mapped.
	 * However, we don't know how far into the page the contents are
	 * valid -- the actual file might be much shorter than the page.
	 * So find out the file size.
 	 */
	error = VOP_GETATTR(imgp->vp, &vattr, imgp->proc->p_ucred, curthread);
	if (error)
		return (error);

	clength = (vattr.va_size > MAXSHELLCMDLEN) ?
	    MAXSHELLCMDLEN : vattr.va_size;
	/*
	 * Figure out the number of bytes that need to be reserved in the
	 * argument string to copy the contents of the interpreter's command
	 * line into the argument string.
	 */
	ihp = &image_header[2];
	offset = 0;
	while (ihp < &image_header[clength]) {
		/* Skip any whitespace */
		if ((*ihp == ' ') || (*ihp == '\t')) {
			ihp++;
			continue;
		}

		/* End of line? */
		if ((*ihp == '\n') || (*ihp == '#') || (*ihp == '\0'))
			break;

		/* Found a token */
		do {
			offset++;
			ihp++;
		} while ((*ihp != ' ') && (*ihp != '\t') && (*ihp != '\n') &&
		    (*ihp != '#') && (*ihp != '\0') &&
		    (ihp < &image_header[clength]));
		/* Include terminating nulls in the offset */
		offset++;
	}

	/* If the script gives a null line as the interpreter, we bail */
	if (offset == 0)
		return (ENOEXEC);

	/* Check that we aren't too big */
	if (ihp == &image_header[MAXSHELLCMDLEN])
		return (ENAMETOOLONG);

	/*
	 * The full path name of the original script file must be tagged
	 * onto the end, adjust the offset to deal with it.
	 *
	 * The original argv[0] is being replaced, set 'length' to the number
	 * of bytes being removed.  So 'offset' is the number of bytes being
	 * added and 'length' is the number of bytes being removed.
	 */
	offset += strlen(imgp->args->fname) + 1;	/* add fname */
	length = (imgp->args->argc == 0) ? 0 :
	    strlen(imgp->args->begin_argv) + 1;		/* bytes to delete */

	if (offset - length > imgp->args->stringspace)
		return (E2BIG);

	bcopy(imgp->args->begin_argv + length, imgp->args->begin_argv + offset,
	    imgp->args->endp - (imgp->args->begin_argv + length));

	offset -= length;		/* calculate actual adjustment */
	imgp->args->begin_envv += offset;
	imgp->args->endp += offset;
	imgp->args->stringspace -= offset;

	/*
	 * If there were no arguments then we've added one, otherwise
	 * decr argc remove old argv[0], incr argc for fname add, net 0
	 */
	if (imgp->args->argc == 0)
		imgp->args->argc = 1;

	/*
	 * Loop through the interpreter name yet again, copying as
	 * we go.
	 */
	ihp = &image_header[2];
	offset = 0;
	while (ihp < &image_header[clength]) {
		/* Skip whitespace */
		if ((*ihp == ' ') || (*ihp == '\t')) {
			ihp++;
			continue;
		}

		/* End of line? */
		if ((*ihp == '\n') || (*ihp == '#') || (*ihp == '\0'))
			break;

		/* Found a token, copy it */
		do {
			imgp->args->begin_argv[offset++] = *ihp++;
		} while ((*ihp != ' ') && (*ihp != '\t') && (*ihp != '\n') &&
		    (*ihp != '#') && (*ihp != '\0') &&
		    (ihp < &image_header[MAXSHELLCMDLEN]));
		imgp->args->begin_argv[offset++] = '\0';
		imgp->args->argc++;
	}

	/*
	 * Finally, add the filename onto the end for the interpreter to
	 * use and copy the interpreter's name to imgp->interpreter_name
	 * for exec to use.
	 */
	error = copystr(imgp->args->fname, imgp->args->buf + offset,
	    imgp->args->stringspace, &length);

	if (error == 0)
		error = copystr(imgp->args->begin_argv, imgp->interpreter_name,
		    MAXSHELLCMDLEN, &length);

	return (error);
}

/*
 * Tell kern_execve.c about it, with a little help from the linker.
 */
static struct execsw shell_execsw = { exec_shell_imgact, "#!" };
EXEC_SET(shell, shell_execsw);
