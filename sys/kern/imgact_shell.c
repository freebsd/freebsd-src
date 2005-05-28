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

#define	KEEP_OLDCODE	1
#if BYTE_ORDER == LITTLE_ENDIAN		/* temp for OLD_CODE kludge */
#define	DBG_MAGIC	0x2B23		/* #+ in "little-endian" */
#define	OLD_MAGIC	0x3C23		/* #< */
#else
#define	DBG_MAGIC	0x232B		/* #+ in big-endian */
#define	OLD_MAGIC	0x233C		/* #< */
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define SHELLMAGIC	0x2123 /* #! */
#else
#define SHELLMAGIC	0x2321
#endif

/*
 * At the time of this writing, MAXSHELLCMDLEN == PAGE_SIZE.  This is
 * significant because the caller has only mapped in one page of the
 * file we're reading.  This code should be changed to know how to
 * read in the second page, but I'm not doing that just yet...
 */
#if MAXSHELLCMDLEN > PAGE_SIZE
#error "MAXSHELLCMDLEN is larger than a single page!"
#endif

/**
 * Shell interpreter image activator. An interpreter name beginning at
 * imgp->args->begin_argv is the minimal successful exit requirement.
 *
 * If the given file is a shell-script, then the first line will start
 * with the two characters `#!' (aka SHELLMAGIC), followed by the name
 * of the shell-interpreter to run, followed by zero or more tokens.
 *
 * The interpreter is then started up such that it will see:
 *    arg[0] -> The name of interpreter as specified after `#!' in the
 *		first line of the script.  The interpreter name must
 *		not be longer than MAXSHELLCMDLEN bytes.
 *    arg[1] -> *If* there are any additional tokens on the first line,
 *		then we add a new arg[1], which is a copy of the rest of
 *		that line.  The copy starts at the first token after the
 *		interpreter name.  We leave it to the interpreter to
 *		parse the tokens in that value.
 *    arg[x] -> the full pathname of the script.  This will either be
 *		arg[2] or arg[1], depending on whether or not tokens
 *		were found after the interpreter name.
 *  arg[x+1] -> all the arguments that were specified on the original
 *		command line.
 *
 * This processing is described in the execve(2) man page.
 */

/*
 * HISTORICAL NOTE: From 1993 to mid-2005, FreeBSD parsed out the tokens as
 * found on the first line of the script, and setup each token as a separate
 * value in arg[].  This extra processing did not match the behavior of other
 * OS's, and caused a few subtle problems.  For one, it meant the kernel was
 * deciding how those values should be parsed (wrt characters for quoting or
 * comments, etc), while the interpreter might have other rules for parsing.
 * It also meant the interpreter had no way of knowing which arguments came
 * from the first line of the shell script, and which arguments were specified
 * by the user on the command line.
 *
 * Only few things in the base system might depend on that non-standard
 * processing (mainly /bin/sh and /usr/bin/env).  And for programs which are
 * not in the base system, the "newer" behavior matches how NetBSD, OpenBSD,
 * Linux, Solaris, AIX, IRIX, and many other Unixes have set up the arg-list
 * for the interpreter.  So if a program can handle this behavior on those
 * other OS's, it should be able to handle it for FreeBSD too.
 */
int
exec_shell_imgact(imgp)
	struct image_params *imgp;
{
	const char *image_header = imgp->image_header;
	const char *ihp, *interpb, *interpe, *maxp, *optb, *opte;
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

	/*
	 * Copy shell name and arguments from image_header into a string
	 *	buffer.  Remember that the caller has mapped only the
	 *	first page of the file into memory.
	 */
	clength = (vattr.va_size > PAGE_SIZE) ? PAGE_SIZE : vattr.va_size;

	maxp = &image_header[clength];
	ihp = &image_header[2];
#if KEEP_OLDCODE
	/*
	 * XXX - Temporarily provide a quick-and-dirty way to get the
	 * older, non-standard option-parsing behavior, just in case
	 * someone finds themselves in an emergency where they need it.
	 * This will not be documented.  It is only for initial testing.
	 */
	if (*(const short *)ihp == OLD_MAGIC)
		ihp += 2;
	else
		goto new_code;
	interpb = ihp;

	/*
	 * Figure out the number of bytes that need to be reserved in the
	 * argument string to copy the contents of the interpreter's command
	 * line into the argument string.
	 */
	ihp = interpb;
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
	ihp = interpb;
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
	goto common_end;
new_code:
#endif
	/*
	 * Find the beginning and end of the interpreter_name.  If the
	 * line does not include any interpreter, or if the name which
	 * was found is too long, we bail out.
	 */
	while (ihp < maxp && ((*ihp == ' ') || (*ihp == '\t')))
		ihp++;
	interpb = ihp;
	while (ihp < maxp && ((*ihp != ' ') && (*ihp != '\t') && (*ihp != '\n')
	    && (*ihp != '\0')))
		ihp++;
	interpe = ihp;
	if (interpb == interpe)
		return (ENOEXEC);
	if ((interpe - interpb) >= MAXSHELLCMDLEN)
		return (ENAMETOOLONG);

	/*
	 * Find the beginning of the options (if any), and the end-of-line.
	 * Then trim the trailing blanks off the value.  Note that some
	 * other operating systems do *not* trim the trailing whitespace...
	 */
	while (ihp < maxp && ((*ihp == ' ') || (*ihp == '\t')))
		ihp++;
	optb = ihp;
	while (ihp < maxp && ((*ihp != '\n') && (*ihp != '\0')))
		ihp++;
	opte = ihp;
	while (--ihp > interpe && ((*ihp == ' ') || (*ihp == '\t')))
		opte = ihp;

	/*
	 * We need to "pop" (remove) the present value of arg[0], and "push"
	 * either two or three new values in the arg[] list.  To do this,
	 * we first shift all the other values in the `begin_argv' area to
	 * provide the exact amount of room for the values added.  Set up
	 * `offset' as the number of bytes to be added to the `begin_argv'
	 * area, and 'length' as the number of bytes being removed.
	 */
	offset = interpe - interpb + 1;			/* interpreter */
	if (opte != optb)				/* options (if any) */
		offset += opte - optb + 1;
	offset += strlen(imgp->args->fname) + 1;	/* fname of script */
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
	 * If there was no arg[0] when we started, then the interpreter_name
	 * is adding an argument (instead of replacing the arg[0] we started
	 * with).  And we're always adding an argument when we include the
	 * full pathname of the original script.
	 */
	if (imgp->args->argc == 0)
		imgp->args->argc = 1;
	imgp->args->argc++;

	/*
	 * The original arg[] list has been shifted appropriately.  Copy in
	 * the interpreter name and options-string.
	 */
	length = interpe - interpb;
	bcopy(interpb, imgp->args->buf, length);
	*(imgp->args->buf + length) = '\0';
	offset = length + 1;
	if (opte != optb) {
		length = opte - optb;
		bcopy(optb, imgp->args->buf + offset, length);
		*(imgp->args->buf + offset + length) = '\0';
		offset += length + 1;
		imgp->args->argc++;
	}

#if KEEP_OLDCODE
common_end:
#endif
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
