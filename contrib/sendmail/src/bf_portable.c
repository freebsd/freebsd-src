/*
 * Copyright (c) 1999-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 * Contributed by Exactis.com, Inc.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: bf_portable.c,v 8.25.4.6 2001/05/03 17:24:01 gshapiro Exp $";
#endif /* ! lint */

#if SFIO
# include <sfio/stdio.h>
#endif /* SFIO */

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <errno.h>
#if !SFIO
# include <stdio.h>
#endif /* !SFIO */
#ifdef BF_STANDALONE
# define sm_free free
# define xalloc malloc
#else /* BF_STANDALONE */
# include "sendmail.h"
#endif /* BF_STANDALONE */
#include "bf_portable.h"
#include "bf.h"

/*
**  BFOPEN -- create a new buffered file
**
**	Parameters:
**		filename -- the file's name
**		fmode -- what mode the file should be created as
**		bsize -- amount of buffer space to allocate (may be 0)
**		flags -- if running under sendmail, passed directly to safeopen
**
**	Returns:
**		a FILE * which may then be used with stdio functions, or NULL
**		on failure. FILE * is opened for writing (mode "w+").
**
**	Side Effects:
**		none.
**
**	Sets errno:
**		ENOMEM -- out of memory
**		ENOENT -- illegal empty filename specified
**		any value of errno specified by open()
**		any value of errno specified by fdopen()
**		any value of errno specified by funopen()
*/

#ifdef BF_STANDALONE
# define OPEN(fn, omode, cmode, sff) open(fn, omode, cmode)
#else /* BF_STANDALONE */
# define OPEN(fn, omode, cmode, sff) safeopen(fn, omode, cmode, sff)
#endif /* BF_STANDALONE */

/* List of currently-open buffered files */
struct bf *bflist = NULL;

FILE *
bfopen(filename, fmode, bsize, flags)
	char *filename;
	int fmode;
	size_t bsize;
	long flags;
{
	struct bf *bfp;
	FILE *retval;
	int fd, l;

	fd = OPEN(filename, O_RDWR | O_CREAT | O_TRUNC, fmode, flags);
	if (fd == -1)
	{
		/* errno is set implicitly by open */
		return NULL;
	}

	retval = fdopen(fd, "w+");

	/* If failure, return immediately */
	if (retval == NULL)
	{
		/* errno is set implicitly by fdopen */
		return NULL;
	}

	/* Allocate memory */
	bfp = (struct bf *)xalloc(sizeof(struct bf));
	if (bfp == NULL)
	{
		(void) fclose(retval);

		/* don't care about errors */
		(void) unlink(filename);
		errno = ENOMEM;
		return NULL;
	}
	if (tTd(58, 8))
		dprintf("bfopen(%s): malloced %ld\n",
			filename, (long) sizeof(struct bf));

	l = strlen(filename) + 1;
	bfp->bf_filename = (char *)xalloc(l);
	if (bfp->bf_filename == NULL)
	{
		sm_free(bfp);
		(void) fclose(retval);

		/* don't care about errors */
		(void) unlink(filename);
		errno = ENOMEM;
		return NULL;
	}
	(void) strlcpy(bfp->bf_filename, filename, l);

	/* Fill in the other fields, then add it to the list */
	bfp->bf_key = retval;
	bfp->bf_committed = FALSE;
	bfp->bf_refcount = 1;

	bfinsert(bfp);

	/* Whew. Nothing bad happened. We're okay. */
	return retval;
}
/*
**  BFDUP -- increase refcount on buffered file
**
**	Parameters:
**		fp -- FILE * to "duplicate"
**
**	Returns:
**		fp with increased refcount
*/

FILE *
bfdup(fp)
	FILE *fp;
{
	struct bf *bfp;

	/* Get associated bf structure */
	bfp = bflookup(fp);

	if (bfp == NULL)
		return NULL;

	/* Increase the refcount */
	bfp->bf_refcount++;

	return fp;
}

/*
**  BFCOMMIT -- "commits" the buffered file
**
**	Parameters:
**		fp -- FILE * to commit to disk
**
**	Returns:
**		0 on success, -1 on error
**
**	Side Effects:
**		Forces the given FILE * to be written to disk if it is not
**		already, and ensures that it will be kept after closing. If
**		fp is not a buffered file, this is a no-op.
**
**	Sets errno:
**		any value of errno specified by open()
**		any value of errno specified by write()
**		any value of errno specified by lseek()
*/

int
bfcommit(fp)
	FILE *fp;
{
	struct bf *bfp;

	/* Get associated bf structure */
	bfp = bflookup(fp);

	/* If called on a normal FILE *, noop */
	if (bfp != NULL)
		bfp->bf_committed = TRUE;

	return 0;
}

/*
**  BFREWIND -- rewinds the FILE *
**
**	Parameters:
**		fp -- FILE * to rewind
**
**	Returns:
**		0 on success, -1 on error
**
**	Side Effects:
**		rewinds the FILE * and puts it into read mode. Normally one
**		would bfopen() a file, write to it, then bfrewind() and
**		fread(). If fp is not a buffered file, this is equivalent to
**		rewind().
**
**	Sets errno:
**		any value of errno specified by fseek()
*/

int
bfrewind(fp)
	FILE *fp;
{
	int err;

	/* check to see if there is an error on the stream */
	err = ferror(fp);
	(void) fflush(fp);

	/*
	**  Clear error if tried to fflush()
	**  a read-only file pointer and
	**  there wasn't a previous error.
	*/

	if (err == 0)
		clearerr(fp);

	/* errno is set implicitly by fseek() before return */
	return fseek(fp, 0, SEEK_SET);
}

/*
**  BFTRUNCATE -- rewinds and truncates the FILE *
**
**	Parameters:
**		fp -- FILE * to truncate
**
**	Returns:
**		0 on success, -1 on error
**
**	Side Effects:
**		rewinds the FILE *, truncates it to zero length, and puts it
**		into write mode. If fp is not a buffered file, this is
**		equivalent to a rewind() and then an ftruncate(fileno(fp), 0).
**
**	Sets errno:
**		any value of errno specified by fseek()
**		any value of errno specified by ftruncate()
*/

int
bftruncate(fp)
	FILE *fp;
{
	int ret;

	if (bfrewind(fp) == -1)
	{
		/* errno is set implicitly by bfrewind() */
		return -1;
	}

#if NOFTRUNCATE
	/* XXX */
	errno = EINVAL;
	ret = -1;
#else /* NOFTRUNCATE */
	/* errno is set implicitly by ftruncate() before return */
	ret = ftruncate(fileno(fp), 0);
#endif /* NOFTRUNCATE */
	return ret;
}

/*
**  BFFSYNC -- fsync the fd associated with the FILE *
**
**	Parameters:
**		fp -- FILE * to fsync
**
**	Returns:
**		0 on success, -1 on error
**
**	Sets errno:
**		EINVAL if FILE * not bfcommitted yet.
**		any value of errno specified by fsync()
*/

int
bffsync(fp)
	FILE *fp;
{
	int fd;
	struct bf *bfp;

	/* Get associated bf structure */
	bfp = bflookup(fp);

	/* If called on a normal FILE *, noop */
	if (bfp != NULL && !bfp->bf_committed)
		fd = -1;
	else
		fd = fileno(fp);

	if (tTd(58, 10))
		dprintf("bffsync: fd = %d\n", fd);

	if (fd < 0)
	{
		errno = EINVAL;
		return -1;
	}
	return fsync(fd);
}

/*
**  BFCLOSE -- close a buffered file
**
**	Parameters:
**		fp -- FILE * to close
**
**	Returns:
**		0 on success, EOF on failure
**
**	Side Effects:
**		Closes fp. If fp is a buffered file, unlink it if it has not
**		already been committed. If fp is not a buffered file, this is
**		equivalent to fclose().
**
**	Sets errno:
**		any value of errno specified by fclose()
*/

int
bfclose(fp)
	FILE *fp;
{
	int retval;
	struct bf *bfp = NULL;

	/* Get associated bf structure */
	bfp = bflookup(fp);

	/* Decrement and check refcount */
	if (bfp != NULL && --bfp->bf_refcount > 0)
		return 0;

	/* If bf, get bf structure and remove from list */
	if (bfp != NULL)
		bfp = bfdelete(fp);

	if (fclose(fp) == EOF)
	{
		if (tTd(58, 8))
			dprintf("bfclose: fclose failed\n");
		/* errno is set implicitly by fclose() */
		return -1;
	}

	if (bfp == NULL)
		return 0;

	/* Success unless we determine otherwise in next block */
	retval = 0;

	if (bfp != NULL)
	{
		/* Might have to unlink; certainly will have to deallocate */
		if (!bfp->bf_committed)
			retval = unlink(bfp->bf_filename);

		sm_free(bfp->bf_filename);
		sm_free(bfp);
		if (tTd(58, 8))
			dprintf("bfclose: freed %ld\n",
				(long) sizeof(struct bf));
	}
	else
	{
		if (tTd(58, 8))
			dprintf("bfclose: bfp was NULL\n");
	}

	return retval;
}

/*
**  BFTEST -- test if a FILE * is a buffered file
**
**	Parameters:
**		fp -- FILE * to test
**
**	Returns:
**		TRUE if fp is a buffered file, FALSE otherwise.
**
**	Side Effects:
**		none.
**
**	Sets errno:
**		never.
*/

bool
bftest(fp)
	FILE *fp;
{
	return (bflookup(fp) != NULL);
}

/*
**  BFINSERT -- insert item in linking list
**
**	Parameters:
**		datum -- item to insert
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
**
**	Sets errno:
**		never.
*/

void
bfinsert(datum)
	struct bf *datum;
{
	datum->bf_cdr = bflist;
	bflist = datum;
}

/*
**  BFLOOKUP -- lookup FILE * in list
**
**	Parameters:
**		fp -- FILE * to lookup
**
**	Returns:
**		bf struct for the FILE *, NULL if not found
**
**	Side Effects:
**		none.
**
**	Sets errno:
**		never.
*/

struct bf *
bflookup(key)
	FILE *key;
{
	struct bf *t;

	for (t = bflist; t != NULL; t = t->bf_cdr)
	{
		if (t->bf_key == key)
		{
			return t;
		}
	}

	/* If we got this far, we didn't find it */
	return NULL;
}

/*
**  BFDELETE -- delete a FILE * in list
**
**	Parameters:
**		fp -- FILE * to delete
**
**	Returns:
**		bf struct for deleted FILE *, NULL if not found,
**
**	Side Effects:
**		none.
**
**	Sets errno:
**		never.
*/

struct bf *
bfdelete(key)
	FILE *key;
{
	struct bf *t, *u;

	if (bflist == NULL)
		return NULL;

	/* if first element, special case */
	if (bflist->bf_key == key)
	{
		u = bflist;
		bflist = bflist->bf_cdr;
		return u;
	}

	for (t = bflist; t->bf_cdr != NULL; t = t->bf_cdr)
	{
		if (t->bf_cdr->bf_key == key)
		{
			u = t->bf_cdr;
			t->bf_cdr = u->bf_cdr;
			return u;
		}
	}

	/* If we got this far, we didn't find it */
	return NULL;
}
