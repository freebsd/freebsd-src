/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$FreeBSD$";
#endif				/* LIBC_SCCS and not lint */

/*
 * This file contains four procedures used to read config-files.
 *
 * char * config_open(const char *filename,int contlines)
 *	Will open the named file, read it into a private malloc'ed area,
 *	and close the file again.
 *	All lines where the first !isspace() char is '#' are deleted.
 *	If contlines are non-zero lines where the first char is isspace()
 *	will be joined to the preceeding line.
 *	In case of trouble the name of the offending system call will be
 *	returned.  On success NULL is returned.
 *
 * void   config_close()
 *	This will free the internal malloc'ed area.
 *
 * char * config_next()
 *	This will return a pointer to the next entry in the area.  NULL is
 *	returned at "end of file".  The return value is '\0' terminated, and
 *	can be modified, but the contents must be copied somewhere else for
 *	permanent use.
 *
 * char * config_skip(char **p)
 *	This will pick out the next word from the string.  The return-value
 *	points to the word found, and *p is advanced past the word.  NULL is
 *	returned at "end of string".
 *
 * Many programs have a n*100 bytes config-file and N*1000 bytes of source
 * to read it.  Doing pointer-aerobics on files that small is a waste of
 * time, and bashing around with getchar/ungetc isn't much better.  These
 * routines implement a simple algorithm and syntax.
 *
 * config_skip consider a contiguous string of !isspace() chars a word.
 *
 * 13nov1994 Poul-Henning Kamp  phk@login.dknet.dk
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

static char *file_buf;
static char *ptr;

char   *
config_open(const char *filename, int contlines)
{
	int     fd;
	struct stat st;
	char   *p, *q;

	if ((fd = open(filename, O_RDONLY)) < 0)
		return "open";
	if (fstat(fd, &st) < 0) {
		close(fd);
		return "fstat";
	}
	if (file_buf)
		free(file_buf);
	file_buf = malloc(st.st_size + 2);
	if (!file_buf) {
		close(fd);
		return "malloc";
	}
	if (st.st_size != read(fd, file_buf, st.st_size)) {
		free(file_buf);
		file_buf = (char *) 0;
		close(fd);
		return "read";
	}
	close(fd);
	file_buf[st.st_size] = '\n';
	file_buf[st.st_size + 1] = '\0';

	/*
         * /^[ \t]*#[^\n]*$/d
	 *
	 * Delete all lines where the first !isspace() char is '#'
         */

	ptr = file_buf;
	for (p = ptr; *p;) {
		for (q = p; *q != '\n' && isspace(*q); q++)
			continue;
		if (*q == '#') {
			p = strchr(p, '\n');
			if (p)
				p++;
		} else {
			q = strchr(p, '\n');
			q++;
			memcpy(ptr, p, q - p);
			ptr += q - p;
			p = q;
		}
	}
	*ptr = '\0';
	ptr = file_buf;

	if (!contlines)
		return 0;

	/* Join all lines starting with a isspace() char to the preceeding
	 * line */

	for (p = ptr; *p;) {
		q = strchr(p, '\n');
		if (isspace(*(q + 1)))
			*q = ' ';
		p = q + 1;
	}

	return 0;
}

void
config_close(void)
{
	if (file_buf)
		free(file_buf);
	ptr = file_buf = 0;
}

/*
 * Get next entry.  config_open did all the weird stuff, so just return
 * the next line.
 */

char   *
config_next(void)
{
	char   *p;

	/* We might be done already ! */
	if (!ptr || !*ptr)
		return 0;

	while (isspace(*ptr))
	    ptr++;
	p = ptr;
	ptr = strchr(p, '\n');
	if (ptr) {
		*ptr = '\0';
		ptr++;
	}
	return p;
}

/*
 * Return next word
 */

char   *
config_skip(char **p)
{
	char   *q, *r;

	if (!*p || !**p)
		return 0;
	for (q = *p; isspace(*q); q++);
	if (!*q)
		return 0;
	for (r = q; *r && !isspace(*r); r++);
	if (*r)
		*r++ = '\0';
	*p = r;
	return q;
}
