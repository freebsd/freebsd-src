/*
 * Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
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
static char id[] = "@(#)$Id: bf_torek.c,v 8.19.18.2 2000/09/17 17:04:26 gshapiro Exp $";
#endif /* ! lint */

#if SFIO
   ERROR README: Can not use bf_torek.c with SFIO.
#endif /* SFIO */

#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#ifndef BF_STANDALONE
# include "sendmail.h"
#endif /* ! BF_STANDALONE */
#include "bf_torek.h"
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

FILE *
bfopen(filename, fmode, bsize, flags)
	char *filename;
	int fmode;
	size_t bsize;
	long flags;
{
	struct bf *bfp;
	FILE *retval;
	int save_errno, l;
	struct stat st;

	/* Sanity checks */
	/* Empty filename string */
	if (*filename == '\0')
	{
		errno = ENOENT;
		return NULL;
	}

	if (stat(filename, &st) == 0)
	{
		/* file already exists on disk */
		errno = EEXIST;
		return NULL;
	}

	/* Allocate memory */
	bfp = (struct bf *)malloc(sizeof(struct bf));
	if (bfp == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	/* A zero bsize is valid, just don't allocate memory */
	if (bsize > 0)
	{
		bfp->bf_buf = (char *)malloc(bsize);
		if (bfp->bf_buf == NULL)
		{
			free(bfp);
			errno = ENOMEM;
			return NULL;
		}
	}
	else
		bfp->bf_buf = NULL;

	/* Nearly home free, just set all the parameters now */
	bfp->bf_committed = FALSE;
	bfp->bf_ondisk = FALSE;
	bfp->bf_refcount = 1;
	bfp->bf_flags = flags;
	bfp->bf_bufsize = bsize;
	bfp->bf_buffilled = 0;
	l = strlen(filename) + 1;
	bfp->bf_filename = (char *)malloc(l);
	if (bfp->bf_filename == NULL)
	{
		free(bfp);
		if (bfp->bf_buf != NULL)
			free(bfp->bf_buf);
		errno = ENOMEM;
		return NULL;
	}
	(void) strlcpy(bfp->bf_filename, filename, l);
	bfp->bf_filemode = fmode;
	bfp->bf_offset = 0;
	bfp->bf_size = 0;

	if (tTd(58, 8))
		dprintf("bfopen(%s, %d)\n", filename, bsize);

	/* The big test: will funopen accept it? */
	retval = funopen((void *)bfp, _bfread, _bfwrite, _bfseek, _bfclose);
	if (retval == NULL)
	{
		/* Just in case free() sets errno */
		save_errno = errno;
		free(bfp);
		free(bfp->bf_filename);
		if (bfp->bf_buf != NULL)
			free(bfp->bf_buf);
		errno = save_errno;
		return NULL;
	}
	else
	{
		/* Success */
		return retval;
	}
}
/*
**  BFDUP -- increase refcount on buffered file
**
**	Parameters:
**		fp -- FILE * to "duplicate"
**
**	Returns:
**		If file is memory buffered, fp with increased refcount
**		If file is on disk, NULL (need to use link())
*/

FILE *
bfdup(fp)
	FILE *fp;
{
	struct bf *bfp;

	/* If called on a normal FILE *, noop */
	if (!bftest(fp))
		return NULL;

	/* Get associated bf structure */
	bfp = (struct bf *)fp->_cookie;

	/* Increase ref count */
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
	int retval;
	int byteswritten;

	/* If called on a normal FILE *, noop */
	if (!bftest(fp))
		return 0;

	/* Get associated bf structure */
	bfp = (struct bf *)fp->_cookie;

	/* If already committed, noop */
	if (bfp->bf_committed)
		return 0;

	/* Do we need to open a file? */
	if (!bfp->bf_ondisk)
	{
		struct stat st;

		if (tTd(58, 8))
			dprintf("bfcommit(%s): to disk\n", bfp->bf_filename);

		if (stat(bfp->bf_filename, &st) == 0)
		{
			errno = EEXIST;
			return -1;
		}

		retval = OPEN(bfp->bf_filename, O_RDWR | O_CREAT | O_TRUNC,
			      bfp->bf_filemode, bfp->bf_flags);

		/* Couldn't create file: failure */
		if (retval < 0)
		{
			/* errno is set implicitly by open() */
			return -1;
		}

		bfp->bf_disk_fd = retval;
		bfp->bf_ondisk = TRUE;
	}

	/* Write out the contents of our buffer, if we have any */
	if (bfp->bf_buffilled > 0)
	{
		byteswritten = 0;

		if (lseek(bfp->bf_disk_fd, 0, SEEK_SET) < 0)
		{
			/* errno is set implicitly by lseek() */
			return -1;
		}

		while (byteswritten < bfp->bf_buffilled)
		{
			retval = write(bfp->bf_disk_fd,
				       bfp->bf_buf + byteswritten,
				       bfp->bf_buffilled - byteswritten);
			if (retval < 0)
			{
				/* errno is set implicitly by write() */
				return -1;
			}
			else
				byteswritten += retval;
		}
	}
	bfp->bf_committed = TRUE;

	/* Invalidate buf; all goes to file now */
	bfp->bf_buffilled = 0;
	if (bfp->bf_bufsize > 0)
	{
		/* Don't need buffer anymore; free it */
		bfp->bf_bufsize = 0;
		free(bfp->bf_buf);
	}
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
	struct bf *bfp;

	if (bfrewind(fp) < 0)
		return -1;

	if (bftest(fp))
	{
		/* Get bf structure */
		bfp = (struct bf *)fp->_cookie;
		bfp->bf_buffilled = 0;
		bfp->bf_size = 0;

		/* Need to zero the buffer */
		if (bfp->bf_bufsize > 0)
			memset(bfp->bf_buf, '\0', bfp->bf_bufsize);
		if (bfp->bf_ondisk)
			return ftruncate(bfp->bf_disk_fd, 0);
		else
			return 0;
	}
	else
		return ftruncate(fileno(fp), 0);
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
	struct bf *bfp;

	/* If called on a normal FILE *, call fclose() on it */
	if (!bftest(fp))
		return fclose(fp);

	/* Cast cookie back to correct type */
	bfp = (struct bf *)fp->_cookie;

	/* Check reference count to see if we actually want to close */
	if (bfp != NULL && --bfp->bf_refcount > 0)
		return 0;

	/*
	**  In this implementation, just call fclose--the _bfclose
	**  routine will be called by that
	*/

	return fclose(fp);
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
	/*
	**  Check to see if our special I/O routines are installed
	**  in this file structure
	*/

	return ((fp->_close == _bfclose) &&
		(fp->_read == _bfread) &&
		(fp->_seek == _bfseek) &&
		(fp->_write == _bfwrite));
}

/*
**  _BFCLOSE -- close a buffered file
**
**	Parameters:
**		cookie -- cookie of file to close
**
**	Returns:
**		0 to indicate success
**
**	Side Effects:
**		deletes backing file, frees memory.
**
**	Sets errno:
**		never.
*/

int
_bfclose(cookie)
	void *cookie;
{
	struct bf *bfp;

	/* Cast cookie back to correct type */
	bfp = (struct bf *)cookie;

	/* Need to clean up the file */
	if (bfp->bf_ondisk && !bfp->bf_committed)
		unlink(bfp->bf_filename);

	/* Need to free the buffer */
	if (bfp->bf_bufsize > 0)
		free(bfp->bf_buf);

	/* Finally, free the structure */
	free(bfp);

	return 0;
}

/*
**  _BFREAD -- read a buffered file
**
**	Parameters:
**		cookie -- cookie of file to read
**		buf -- buffer to fill
**		nbytes -- how many bytes to read
**
**	Returns:
**		number of bytes read or -1 indicate failure
**
**	Side Effects:
**		none.
**
*/

int
_bfread(cookie, buf, nbytes)
	void *cookie;
	char *buf;
	int nbytes;
{
	struct bf *bfp;
	int count = 0;	/* Number of bytes put in buf so far */
	int retval;

	/* Cast cookie back to correct type */
	bfp = (struct bf *)cookie;

	if (bfp->bf_offset < bfp->bf_buffilled)
	{
		/* Need to grab some from buffer */
		count = nbytes;
		if ((bfp->bf_offset + count) > bfp->bf_buffilled)
			count = bfp->bf_buffilled - bfp->bf_offset;

		memcpy(buf, bfp->bf_buf + bfp->bf_offset, count);
	}

	if ((bfp->bf_offset + nbytes) > bfp->bf_buffilled)
	{
		/* Need to grab some from file */

		if (!bfp->bf_ondisk)
		{
			/* Oops, the file doesn't exist. EOF. */
			goto finished;
		}

		/* Catch a read() on an earlier failed write to disk */
		if (bfp->bf_disk_fd < 0)
		{
			errno = EIO;
			return -1;
		}

		if (lseek(bfp->bf_disk_fd,
			  bfp->bf_offset + count, SEEK_SET) < 0)
		{
			if ((errno == EINVAL) || (errno == ESPIPE))
			{
				/*
				**  stdio won't be expecting these
				**  errnos from read()! Change them
				**  into something it can understand.
				*/

				errno = EIO;
			}
			return -1;
		}

		while (count < nbytes)
		{
			retval = read(bfp->bf_disk_fd,
				      buf + count,
				      nbytes - count);
			if (retval < 0)
			{
				/* errno is set implicitly by read() */
				return -1;
			}
			else if (retval == 0)
				goto finished;
			else
				count += retval;
		}
	}

finished:
	bfp->bf_offset += count;
	return count;
}

/*
**  _BFSEEK -- seek to a position in a buffered file
**
**	Parameters:
**		cookie -- cookie of file to seek
**		offset -- position to seek to
**		whence -- how to seek
**
**	Returns:
**		new file offset or -1 indicate failure
**
**	Side Effects:
**		none.
**
*/

fpos_t
_bfseek(cookie, offset, whence)
	void *cookie;
	fpos_t offset;
	int whence;

{
	struct bf *bfp;

	/* Cast cookie back to correct type */
	bfp = (struct bf *)cookie;

	switch (whence)
	{
		case SEEK_SET:
			bfp->bf_offset = offset;
			break;

		case SEEK_CUR:
			bfp->bf_offset += offset;
			break;

		case SEEK_END:
			bfp->bf_offset = bfp->bf_size + offset;
			break;

		default:
			errno = EINVAL;
			return -1;
	}
	return bfp->bf_offset;
}

/*
**  _BFWRITE -- write to a buffered file
**
**	Parameters:
**		cookie -- cookie of file to write
**		buf -- data buffer
**		nbytes -- how many bytes to write
**
**	Returns:
**		number of bytes written or -1 indicate failure
**
**	Side Effects:
**		may create backing file if over memory limit for file.
**
*/

int
_bfwrite(cookie, buf, nbytes)
	void *cookie;
	const char *buf;
	int nbytes;
{
	struct bf *bfp;
	int count = 0;	/* Number of bytes written so far */
	int retval;

	/* Cast cookie back to correct type */
	bfp = (struct bf *)cookie;

	/* If committed, go straight to disk */
	if (bfp->bf_committed)
	{
		if (lseek(bfp->bf_disk_fd, bfp->bf_offset, SEEK_SET) < 0)
		{
			if ((errno == EINVAL) || (errno == ESPIPE))
			{
				/*
				**  stdio won't be expecting these
				**  errnos from write()! Change them
				**  into something it can understand.
				*/

				errno = EIO;
			}
			return -1;
		}

		count = write(bfp->bf_disk_fd, buf, nbytes);
		if (count < 0)
		{
			/* errno is set implicitly by write() */
			return -1;
		}
		goto finished;
	}

	if (bfp->bf_offset < bfp->bf_bufsize)
	{
		/* Need to put some in buffer */
		count = nbytes;
		if ((bfp->bf_offset + count) > bfp->bf_bufsize)
			count = bfp->bf_bufsize - bfp->bf_offset;

		memcpy(bfp->bf_buf + bfp->bf_offset, buf, count);
		if ((bfp->bf_offset + count) > bfp->bf_buffilled)
			bfp->bf_buffilled = bfp->bf_offset + count;
	}

	if ((bfp->bf_offset + nbytes) > bfp->bf_bufsize)
	{
		/* Need to put some in file */
		if (!bfp->bf_ondisk)
		{
			/* Oops, the file doesn't exist. */
			if (tTd(58, 8))
				dprintf("_bfwrite(%s): to disk\n",
					bfp->bf_filename);

			retval = OPEN(bfp->bf_filename,
				      O_RDWR | O_CREAT | O_TRUNC,
				      bfp->bf_filemode, bfp->bf_flags);

			/* Couldn't create file: failure */
			if (retval < 0)
			{
				/*
				**  stdio may not be expecting these
				**  errnos from write()! Change to
				**  something which it can understand.
				**  Note that ENOSPC and EDQUOT are saved
				**  because they are actually valid for
				**  write().
				*/

				if (!((errno == ENOSPC) || (errno == EDQUOT)))
					errno = EIO;

				return -1;
			}
			bfp->bf_disk_fd = retval;
			bfp->bf_ondisk = TRUE;
		}

		/* Catch a write() on an earlier failed write to disk */
		if (bfp->bf_ondisk && bfp->bf_disk_fd < 0)
		{
			errno = EIO;
			return -1;
		}

		if (lseek(bfp->bf_disk_fd,
			  bfp->bf_offset + count, SEEK_SET) < 0)
		{
			if ((errno == EINVAL) || (errno == ESPIPE))
			{
				/*
				**  stdio won't be expecting these
				**  errnos from write()! Change them into
				**  something which it can understand.
				*/

				errno = EIO;
			}
			return -1;
		}

		while (count < nbytes)
		{
			retval = write(bfp->bf_disk_fd, buf + count,
				       nbytes - count);
			if (retval < 0)
			{
				/* errno is set implicitly by write() */
				return -1;
			}
			else
				count += retval;
		}
	}

finished:
	bfp->bf_offset += count;
	if (bfp->bf_offset > bfp->bf_size)
		bfp->bf_size = bfp->bf_offset;
	return count;
}
