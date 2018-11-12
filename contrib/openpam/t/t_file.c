/*-
 * Copyright (c) 2012 Dag-Erling Smørgrav
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $Id: t_file.c 648 2013-03-05 17:54:27Z des $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "openpam_asprintf.h"

#include "t.h"

static struct t_file *tflist;

/*
 * Open a temp file.
 */
struct t_file *
t_fopen(const char *filename)
{
	struct t_file *tf;
	int fd;

	if ((tf = calloc(sizeof *tf, 1)) == NULL)
		err(1, "%s(): calloc()", __func__);
	if (filename) {
		if ((tf->name = strdup(filename)) == NULL)
			err(1, "%s(): strdup()", __func__);
	} else {
		asprintf(&tf->name, "%s.%lu.%p.tmp",
		    t_progname, (unsigned long)getpid(), (void *)tf);
		if (tf->name == NULL)
			err(1, "%s(): asprintf()", __func__);
	}
	if ((fd = open(tf->name, O_RDWR|O_CREAT|O_TRUNC, 0600)) < 0)
		err(1, "%s(): %s", __func__, tf->name);
	if ((tf->file = fdopen(fd, "r+")) == NULL)
		err(1, "%s(): fdopen()", __func__);
	if ((tf->next = tflist) != NULL)
		tf->next->prev = tf;
	tflist = tf;
	return (tf);
}

/*
 * Write text to the temp file.
 */
int
t_fprintf(struct t_file *tf, const char *fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = vfprintf(tf->file, fmt, ap);
	va_end(ap);
	if (ferror(tf->file))
		err(1, "%s(): vfprintf()", __func__);
	return (len);
}

/*
 * Rewind the temp file.
 */
void
t_frewind(struct t_file *tf)
{

	errno = 0;
	rewind(tf->file);
	if (errno != 0)
		err(1, "%s(): rewind()", __func__);
}

/*
 * Return non-zero if an error occurred.
 */
int
t_ferror(struct t_file *tf)
{

	return (ferror(tf->file));
}

/*
 * Return non-zero if the end of the file was reached.
 */
int
t_feof(struct t_file *tf)
{

	return (feof(tf->file));
}

/*
 * Close a temp file.
 */
void
t_fclose(struct t_file *tf)
{

	if (tf == tflist)
		tflist = tf->next;
	if (tf->prev)
		tf->prev->next = tf->next;
	if (tf->next)
		tf->next->prev = tf->prev;
	fclose(tf->file);
	if (unlink(tf->name) < 0)
		warn("%s(): unlink()", __func__);
	free(tf->name);
	free(tf);
}

/*
 * atexit() function to close all remaining files.
 */
void
t_fcloseall(void)
{

	while (tflist)
		t_fclose(tflist);
}
