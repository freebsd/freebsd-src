/*
 * Copyright (c) 1996, Gary J. Palmer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
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
 *	$Id: ctm_dequeue.c,v 1.3 1996/09/07 18:48:42 peter Exp $
 */

/* 
 * Change this if you want to alter how many files it sends out by
 * default
 */

#define DEFAULT_NUM 2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <sys/mman.h>
#include <errno.h>
#include <paths.h>
#include "error.h"
#include "options.h"

int fts_sort(const FTSENT **, const FTSENT **);
FILE *open_sendmail(void);
int close_sendmail(FILE *fp);

int
main(int argc, char **argv)
{
    char *log_file = NULL;
    char *queue_dir = NULL;
    char *list[2];
    char *buffer, *filename;
    int num_to_send = DEFAULT_NUM, piece, fp, len;
    FTS *fts;
    FTSENT *ftsent;
    FILE *sfp;

    err_prog_name(argv[0]);

    OPTIONS("[-l log] [-n num] queuedir")
	NUMBER('n', num_to_send)
	STRING('l', log_file)
    ENDOPTS;

    if (argc != 2)
	usage();

    queue_dir = argv[1];
    list[0] = queue_dir;
    list[1] = NULL;

    fts = fts_open(list, FTS_PHYSICAL, fts_sort);
    if (fts == NULL)
    {
	err("fts failed on `%s'", queue_dir);
	exit(1);
    }

    (void) fts_read(fts);

    ftsent = fts_children(fts, 0);
    if (ftsent == NULL)
    {
	if (errno) {
	    err("ftschildren failed");
	    exit(1);
	} else
	    exit(0);
    }

    /* assumption :-( */
    len = strlen(queue_dir) + 40;
    filename = malloc(len);
    if (filename == NULL)
    {
	err("malloc failed");
	exit(1);
    }

    for (piece = 0; piece < num_to_send ; piece++)
    {
	/* Skip non-files and files we should ignore (ones starting with `.') */

#define ISFILE ((ftsent->fts_info & FTS_F) == FTS_F)
#define IGNORE (ftsent->fts_name[0] == '.')
#define HASNEXT (ftsent->fts_link != NULL)

	while(((!ISFILE) || (IGNORE)) && (HASNEXT))
	    ftsent = ftsent->fts_link;

	if ((!ISFILE) || (IGNORE))
	{
	    err("No more chunks to mail");
	    exit(0);
	}

#undef ISFILE
#undef IGNORE
#undef HASNEXT

	if (snprintf(filename, len, "%s/%s", queue_dir, ftsent->fts_name) > len)
	    err("snprintf(filename) longer than buffer");

	fp = open(filename, O_RDONLY, 0);
	if (fp <  0)
	{
	    err("open(`%s') failed, errno = %d", filename, errno);
	    exit(1);
	}

	buffer = mmap(0, ftsent->fts_statp->st_size, PROT_READ, MAP_PRIVATE, fp, 0);
	if (((int) buffer) <= 0)
	{
	    err("mmap failed, errno = %d", errno);
	    exit(1);
	}

	sfp = open_sendmail();	    
	if (sfp == NULL)
	    exit(1);
	
	if (fwrite(buffer, ftsent->fts_statp->st_size, 1, sfp) < 1)
	{
	    err("fwrite failed: errno = %d", errno);
	    close_sendmail(sfp);
	    exit(1);
	}

	if (!close_sendmail(sfp))
	    exit(1);

	munmap(buffer, ftsent->fts_statp->st_size);
	close(fp);

	if (unlink(filename) < 0)
	{
	    err("unlink of `%s' failed", filename);
	    exit(1);
	}
	
	err("sent file `%s'", ftsent->fts_name);

	if (ftsent->fts_link != NULL)
	    ftsent = ftsent->fts_link;
	else
	    break;
    }

    err("exiting normally");
    return(0);
}

int
fts_sort(const FTSENT ** a, const FTSENT ** b)
{
	int a_info, b_info;

	a_info = (*a)->fts_info;
	if (a_info == FTS_ERR)
		return (0);
	b_info = (*b)->fts_info;
	if (b_info == FTS_ERR)
		return (0);

	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

/*
 * Start a pipe to sendmail.  Sendmail will decode the destination
 * from the message contents.
 */
FILE *
open_sendmail()
{
    FILE *fp;
    char buf[100];
    
    sprintf(buf, "%s -odq -t", _PATH_SENDMAIL);
    if ((fp = popen(buf, "w")) == NULL)
	err("cannot start sendmail");
    return fp;
}


/*
 * Close a pipe to sendmail.  Sendmail will then do its bit.
 * Return 1 on success, 0 on failure.
 */
int
close_sendmail(FILE *fp)
{
    int status;
    
    fflush(fp);
    if (ferror(fp))
    {
	err("error writing to sendmail");
	return 0;
    }
    
    if ((status = pclose(fp)) != 0)
	err("sendmail failed with status %d", status);
    
    return (status == 0);
}
