/* Code for the buffer data structure.  */

#include <assert.h>
#include "cvs.h"
#include "buffer.h"

#if defined (SERVER_SUPPORT) || defined (CLIENT_SUPPORT)

/* OS/2 doesn't have EIO.  FIXME: this whole notion of turning
   a different error into EIO strikes me as pretty dubious.  */
#if !defined (EIO)
#define EIO EBADPOS
#endif

/* Linked list of available buffer_data structures.  */
static struct buffer_data *free_buffer_data;

/* Local functions.  */
static void allocate_buffer_datas PROTO((void));
static struct buffer_data *get_buffer_data PROTO((void));

/* Initialize a buffer structure.  */

struct buffer *
buf_initialize (input, output, flush, block, shutdown, memory, closure)
     int (*input) PROTO((void *, char *, int, int, int *));
     int (*output) PROTO((void *, const char *, int, int *));
     int (*flush) PROTO((void *));
     int (*block) PROTO((void *, int));
     int (*shutdown) PROTO((void *));
     void (*memory) PROTO((struct buffer *));
     void *closure;
{
    struct buffer *buf;

    buf = (struct buffer *) xmalloc (sizeof (struct buffer));
    buf->data = NULL;
    buf->last = NULL;
    buf->nonblocking = 0;
    buf->input = input;
    buf->output = output;
    buf->flush = flush;
    buf->block = block;
    buf->shutdown = shutdown;
    buf->memory_error = memory;
    buf->closure = closure;
    return buf;
}

/* Initialize a buffer structure which is not to be used for I/O.  */

struct buffer *
buf_nonio_initialize (memory)
     void (*memory) PROTO((struct buffer *));
{
    return (buf_initialize
	    ((int (*) PROTO((void *, char *, int, int, int *))) NULL,
	     (int (*) PROTO((void *, const char *, int, int *))) NULL,
	     (int (*) PROTO((void *))) NULL,
	     (int (*) PROTO((void *, int))) NULL,
	     (int (*) PROTO((void *))) NULL,
	     memory,
	     (void *) NULL));
}

/* Allocate more buffer_data structures.  */

static void
allocate_buffer_datas ()
{
    struct buffer_data *alc;
    char *space;
    int i;

    /* Allocate buffer_data structures in blocks of 16.  */
#define ALLOC_COUNT (16)

    alc = ((struct buffer_data *)
	   malloc (ALLOC_COUNT * sizeof (struct buffer_data)));
    space = (char *) valloc (ALLOC_COUNT * BUFFER_DATA_SIZE);
    if (alc == NULL || space == NULL)
	return;
    for (i = 0; i < ALLOC_COUNT; i++, alc++, space += BUFFER_DATA_SIZE)
    {
	alc->next = free_buffer_data;
	free_buffer_data = alc;
	alc->text = space;
    }	  
}

/* Get a new buffer_data structure.  */

static struct buffer_data *
get_buffer_data ()
{
    struct buffer_data *ret;

    if (free_buffer_data == NULL)
    {
	allocate_buffer_datas ();
	if (free_buffer_data == NULL)
	    return NULL;
    }

    ret = free_buffer_data;
    free_buffer_data = ret->next;
    return ret;
}

/* See whether a buffer is empty.  */

int
buf_empty_p (buf)
    struct buffer *buf;
{
    struct buffer_data *data;

    for (data = buf->data; data != NULL; data = data->next)
	if (data->size > 0)
	    return 0;
    return 1;
}

#ifdef SERVER_FLOWCONTROL
/*
 * Count how much data is stored in the buffer..
 * Note that each buffer is a malloc'ed chunk BUFFER_DATA_SIZE.
 */

int
buf_count_mem (buf)
    struct buffer *buf;
{
    struct buffer_data *data;
    int mem = 0;

    for (data = buf->data; data != NULL; data = data->next)
	mem += BUFFER_DATA_SIZE;

    return mem;
}
#endif /* SERVER_FLOWCONTROL */

/* Add data DATA of length LEN to BUF.  */

void
buf_output (buf, data, len)
    struct buffer *buf;
    const char *data;
    int len;
{
    if (buf->data != NULL
	&& (((buf->last->text + BUFFER_DATA_SIZE)
	     - (buf->last->bufp + buf->last->size))
	    >= len))
    {
	memcpy (buf->last->bufp + buf->last->size, data, len);
	buf->last->size += len;
	return;
    }

    while (1)
    {
	struct buffer_data *newdata;

	newdata = get_buffer_data ();
	if (newdata == NULL)
	{
	    (*buf->memory_error) (buf);
	    return;
	}

	if (buf->data == NULL)
	    buf->data = newdata;
	else
	    buf->last->next = newdata;
	newdata->next = NULL;
	buf->last = newdata;

	newdata->bufp = newdata->text;

	if (len <= BUFFER_DATA_SIZE)
	{
	    newdata->size = len;
	    memcpy (newdata->text, data, len);
	    return;
	}

	newdata->size = BUFFER_DATA_SIZE;
	memcpy (newdata->text, data, BUFFER_DATA_SIZE);

	data += BUFFER_DATA_SIZE;
	len -= BUFFER_DATA_SIZE;
    }

    /*NOTREACHED*/
}

/* Add a '\0' terminated string to BUF.  */

void
buf_output0 (buf, string)
    struct buffer *buf;
    const char *string;
{
    buf_output (buf, string, strlen (string));
}

/* Add a single character to BUF.  */

void
buf_append_char (buf, ch)
    struct buffer *buf;
    int ch;
{
    if (buf->data != NULL
	&& (buf->last->text + BUFFER_DATA_SIZE
	    != buf->last->bufp + buf->last->size))
    {
	*(buf->last->bufp + buf->last->size) = ch;
	++buf->last->size;
    }
    else
    {
	char b;

	b = ch;
	buf_output (buf, &b, 1);
    }
}

/*
 * Send all the output we've been saving up.  Returns 0 for success or
 * errno code.  If the buffer has been set to be nonblocking, this
 * will just write until the write would block.
 */

int
buf_send_output (buf)
     struct buffer *buf;
{
    if (buf->output == NULL)
	abort ();

    while (buf->data != NULL)
    {
	struct buffer_data *data;

	data = buf->data;

	if (data->size > 0)
	{
	    int status, nbytes;

	    status = (*buf->output) (buf->closure, data->bufp, data->size,
				     &nbytes);
	    if (status != 0)
	    {
		/* Some sort of error.  Discard the data, and return.  */

		buf->last->next = free_buffer_data;
		free_buffer_data = buf->data;
		buf->data = NULL;
		buf->last = NULL;

	        return status;
	    }

	    if (nbytes != data->size)
	    {
		/* Not all the data was written out.  This is only
                   permitted in nonblocking mode.  Adjust the buffer,
                   and return.  */

		assert (buf->nonblocking);

		data->size -= nbytes;
		data->bufp += nbytes;

		return 0;
	    }
	}

	buf->data = data->next;
	data->next = free_buffer_data;
	free_buffer_data = data;
    }

    buf->last = NULL;

    return 0;
}

/*
 * Flush any data queued up in the buffer.  If BLOCK is nonzero, then
 * if the buffer is in nonblocking mode, put it into blocking mode for
 * the duration of the flush.  This returns 0 on success, or an error
 * code.
 */

int
buf_flush (buf, block)
     struct buffer *buf;
     int block;
{
    int nonblocking;
    int status;

    if (buf->flush == NULL)
        abort ();

    nonblocking = buf->nonblocking;
    if (nonblocking && block)
    {
        status = set_block (buf);
	if (status != 0)
	    return status;
    }

    status = buf_send_output (buf);
    if (status == 0)
        status = (*buf->flush) (buf->closure);

    if (nonblocking && block)
    {
        int blockstat;

        blockstat = set_nonblock (buf);
	if (status == 0)
	    status = blockstat;
    }

    return status;
}

/*
 * Set buffer BUF to nonblocking I/O.  Returns 0 for success or errno
 * code.
 */

int
set_nonblock (buf)
     struct buffer *buf;
{
    int status;

    if (buf->nonblocking)
	return 0;
    if (buf->block == NULL)
        abort ();
    status = (*buf->block) (buf->closure, 0);
    if (status != 0)
	return status;
    buf->nonblocking = 1;
    return 0;
}

/*
 * Set buffer BUF to blocking I/O.  Returns 0 for success or errno
 * code.
 */

int
set_block (buf)
     struct buffer *buf;
{
    int status;

    if (! buf->nonblocking)
	return 0;
    if (buf->block == NULL)
        abort ();
    status = (*buf->block) (buf->closure, 1);
    if (status != 0)
	return status;
    buf->nonblocking = 0;
    return 0;
}

/*
 * Send a character count and some output.  Returns errno code or 0 for
 * success.
 *
 * Sending the count in binary is OK since this is only used on a pipe
 * within the same system.
 */

int
buf_send_counted (buf)
     struct buffer *buf;
{
    int size;
    struct buffer_data *data;

    size = 0;
    for (data = buf->data; data != NULL; data = data->next)
	size += data->size;

    data = get_buffer_data ();
    if (data == NULL)
    {
	(*buf->memory_error) (buf);
	return ENOMEM;
    }

    data->next = buf->data;
    buf->data = data;
    if (buf->last == NULL)
	buf->last = data;

    data->bufp = data->text;
    data->size = sizeof (int);

    *((int *) data->text) = size;

    return buf_send_output (buf);
}

/*
 * Send a special count.  COUNT should be negative.  It will be
 * handled speciallyi by buf_copy_counted.  This function returns 0 or
 * an errno code.
 *
 * Sending the count in binary is OK since this is only used on a pipe
 * within the same system.
 */

int
buf_send_special_count (buf, count)
     struct buffer *buf;
     int count;
{
    struct buffer_data *data;

    data = get_buffer_data ();
    if (data == NULL)
    {
	(*buf->memory_error) (buf);
	return ENOMEM;
    }

    data->next = buf->data;
    buf->data = data;
    if (buf->last == NULL)
	buf->last = data;

    data->bufp = data->text;
    data->size = sizeof (int);

    *((int *) data->text) = count;

    return buf_send_output (buf);
}

/* Append a list of buffer_data structures to an buffer.  */

void
buf_append_data (buf, data, last)
     struct buffer *buf;
     struct buffer_data *data;
     struct buffer_data *last;
{
    if (data != NULL)
    {
	if (buf->data == NULL)
	    buf->data = data;
	else
	    buf->last->next = data;
	buf->last = last;
    }
}

/*
 * Copy the contents of file F into buffer_data structures.  We can't
 * copy directly into an buffer, because we want to handle failure and
 * succeess differently.  Returns 0 on success, or -2 if out of
 * memory, or a status code on error.  Since the caller happens to
 * know the size of the file, it is passed in as SIZE.  On success,
 * this function sets *RETP and *LASTP, which may be passed to
 * buf_append_data.
 */

int
buf_read_file (f, size, retp, lastp)
    FILE *f;
    long size;
    struct buffer_data **retp;
    struct buffer_data **lastp;
{
    int status;

    *retp = NULL;
    *lastp = NULL;

    while (size > 0)
    {
	struct buffer_data *data;
	int get;

	data = get_buffer_data ();
	if (data == NULL)
	{
	    status = -2;
	    goto error_return;
	}

	if (*retp == NULL)
	    *retp = data;
	else
	    (*lastp)->next = data;
	data->next = NULL;
	*lastp = data;

	data->bufp = data->text;
	data->size = 0;

	if (size > BUFFER_DATA_SIZE)
	    get = BUFFER_DATA_SIZE;
	else
	    get = size;

	errno = EIO;
	if (fread (data->text, get, 1, f) != 1)
	{
	    status = errno;
	    goto error_return;
	}

	data->size += get;
	size -= get;
    }

    return 0;

  error_return:
    if (*retp != NULL)
    {
	(*lastp)->next = free_buffer_data;
	free_buffer_data = *retp;
    }
    return status;
}

/*
 * Copy the contents of file F into buffer_data structures.  We can't
 * copy directly into an buffer, because we want to handle failure and
 * succeess differently.  Returns 0 on success, or -2 if out of
 * memory, or a status code on error.  On success, this function sets
 * *RETP and *LASTP, which may be passed to buf_append_data.
 */

int
buf_read_file_to_eof (f, retp, lastp)
     FILE *f;
     struct buffer_data **retp;
     struct buffer_data **lastp;
{
    int status;

    *retp = NULL;
    *lastp = NULL;

    while (!feof (f))
    {
	struct buffer_data *data;
	int get, nread;

	data = get_buffer_data ();
	if (data == NULL)
	{
	    status = -2;
	    goto error_return;
	}

	if (*retp == NULL)
	    *retp = data;
	else
	    (*lastp)->next = data;
	data->next = NULL;
	*lastp = data;

	data->bufp = data->text;
	data->size = 0;

	get = BUFFER_DATA_SIZE;

	errno = EIO;
	nread = fread (data->text, 1, get, f);
	if (nread == 0 && !feof (f))
	{
	    status = errno;
	    goto error_return;
	}

	data->size = nread;
    }

    return 0;

  error_return:
    if (*retp != NULL)
    {
	(*lastp)->next = free_buffer_data;
	free_buffer_data = *retp;
    }
    return status;
}

/* Return the number of bytes in a chain of buffer_data structures.  */

int
buf_chain_length (buf)
     struct buffer_data *buf;
{
    int size = 0;
    while (buf)
    {
	size += buf->size;
	buf = buf->next;
    }
    return size;
}

/*
 * Read an arbitrary amount of data into an input buffer.  The buffer
 * will be in nonblocking mode, and we just grab what we can.  Return
 * 0 on success, or -1 on end of file, or -2 if out of memory, or an
 * error code.  If COUNTP is not NULL, *COUNTP is set to the number of
 * bytes read.
 */

int
buf_input_data (buf, countp)
     struct buffer *buf;
     int *countp;
{
    if (buf->input == NULL)
	abort ();

    if (countp != NULL)
	*countp = 0;

    while (1)
    {
	int get;
	int status, nbytes;

	if (buf->data == NULL
	    || (buf->last->bufp + buf->last->size
		== buf->last->text + BUFFER_DATA_SIZE))
	{
	    struct buffer_data *data;

	    data = get_buffer_data ();
	    if (data == NULL)
	    {
		(*buf->memory_error) (buf);
		return -2;
	    }

	    if (buf->data == NULL)
		buf->data = data;
	    else
		buf->last->next = data;
	    data->next = NULL;
	    buf->last = data;

	    data->bufp = data->text;
	    data->size = 0;
	}

	get = ((buf->last->text + BUFFER_DATA_SIZE)
	       - (buf->last->bufp + buf->last->size));

	status = (*buf->input) (buf->closure,
				buf->last->bufp + buf->last->size,
				0, get, &nbytes);
	if (status != 0)
	    return status;

	buf->last->size += nbytes;
	if (countp != NULL)
	    *countp += nbytes;

	if (nbytes < get)
	{
	    /* If we did not fill the buffer, then presumably we read
               all the available data.  */
	    return 0;
	}
    }

    /*NOTREACHED*/
}

/*
 * Read a line (characters up to a \012) from an input buffer.  (We
 * use \012 rather than \n for the benefit of non Unix clients for
 * which \n means something else).  This returns 0 on success, or -1
 * on end of file, or -2 if out of memory, or an error code.  If it
 * succeeds, it sets *LINE to an allocated buffer holding the contents
 * of the line.  The trailing \012 is not included in the buffer.  If
 * LENP is not NULL, then *LENP is set to the number of bytes read;
 * strlen may not work, because there may be embedded null bytes.
 */

int
buf_read_line (buf, line, lenp)
     struct buffer *buf;
     char **line;
     int *lenp;
{
    if (buf->input == NULL)
        abort ();

    *line = NULL;

    while (1)
    {
	int len, finallen = 0;
	struct buffer_data *data;
	char *nl;

	/* See if there is a newline in BUF.  */
	len = 0;
	for (data = buf->data; data != NULL; data = data->next)
	{
	    nl = memchr (data->bufp, '\012', data->size);
	    if (nl != NULL)
	    {
	        finallen = nl - data->bufp;
	        len += finallen;
		break;
	    }
	    len += data->size;
	}

	/* If we found a newline, copy the line into a memory buffer,
           and remove it from BUF.  */
	if (data != NULL)
	{
	    char *p;
	    struct buffer_data *nldata;

	    p = malloc (len + 1);
	    if (p == NULL)
		return -2;
	    *line = p;

	    nldata = data;
	    data = buf->data;
	    while (data != nldata)
	    {
		struct buffer_data *next;

		memcpy (p, data->bufp, data->size);
		p += data->size;
		next = data->next;
		data->next = free_buffer_data;
		free_buffer_data = data;
		data = next;
	    }

	    memcpy (p, data->bufp, finallen);
	    p[finallen] = '\0';

	    data->size -= finallen + 1;
	    data->bufp = nl + 1;
	    buf->data = data;

	    if (lenp != NULL)
	        *lenp = len;

	    return 0;
	}

	/* Read more data until we get a newline.  */
	while (1)
	{
	    int size, status, nbytes;
	    char *mem;

	    if (buf->data == NULL
		|| (buf->last->bufp + buf->last->size
		    == buf->last->text + BUFFER_DATA_SIZE))
	    {
		data = get_buffer_data ();
		if (data == NULL)
		{
		    (*buf->memory_error) (buf);
		    return -2;
		}

		if (buf->data == NULL)
		    buf->data = data;
		else
		    buf->last->next = data;
		data->next = NULL;
		buf->last = data;

		data->bufp = data->text;
		data->size = 0;
	    }

	    mem = buf->last->bufp + buf->last->size;
	    size = (buf->last->text + BUFFER_DATA_SIZE) - mem;

	    /* We need to read at least 1 byte.  We can handle up to
               SIZE bytes.  This will only be efficient if the
               underlying communication stream does its own buffering,
               or is clever about getting more than 1 byte at a time.  */
	    status = (*buf->input) (buf->closure, mem, 1, size, &nbytes);
	    if (status != 0)
		return status;

	    buf->last->size += nbytes;

	    /* Optimize slightly to avoid an unnecessary call to
               memchr.  */
	    if (nbytes == 1)
	    {
		if (*mem == '\012')
		    break;
	    }
	    else
	    {
		if (memchr (mem, '\012', nbytes) != NULL)
		    break;
	    }
	}
    }
}

/*
 * Extract data from the input buffer BUF.  This will read up to WANT
 * bytes from the buffer.  It will set *RETDATA to point at the bytes,
 * and set *GOT to the number of bytes to be found there.  Any buffer
 * call which uses BUF may change the contents of the buffer at *DATA,
 * so the data should be fully processed before any further calls are
 * made.  This returns 0 on success, or -1 on end of file, or -2 if
 * out of memory, or an error code.
 */

int
buf_read_data (buf, want, retdata, got)
     struct buffer *buf;
     int want;
     char **retdata;
     int *got;
{
    if (buf->input == NULL)
	abort ();

    while (buf->data != NULL && buf->data->size == 0)
    {
	struct buffer_data *next;

	next = buf->data->next;
	buf->data->next = free_buffer_data;
	free_buffer_data = buf->data;
	buf->data = next;
	if (next == NULL)
	    buf->last = NULL;
    }

    if (buf->data == NULL)
    {
	struct buffer_data *data;
	int get, status, nbytes;

	data = get_buffer_data ();
	if (data == NULL)
	{
	    (*buf->memory_error) (buf);
	    return -2;
	}

	buf->data = data;
	buf->last = data;
	data->next = NULL;
	data->bufp = data->text;
	data->size = 0;

	if (want < BUFFER_DATA_SIZE)
	    get = want;
	else
	    get = BUFFER_DATA_SIZE;
	status = (*buf->input) (buf->closure, data->bufp, get,
				BUFFER_DATA_SIZE, &nbytes);
	if (status != 0)
	    return status;

	data->size = nbytes;
    }

    *retdata = buf->data->bufp;
    if (want < buf->data->size)
    {
        *got = want;
	buf->data->size -= want;
	buf->data->bufp += want;
    }
    else
    {
        *got = buf->data->size;
	buf->data->size = 0;
    }

    return 0;
}

/*
 * Copy lines from an input buffer to an output buffer.  This copies
 * all complete lines (characters up to a newline) from INBUF to
 * OUTBUF.  Each line in OUTBUF is preceded by the character COMMAND
 * and a space.
 */

void
buf_copy_lines (outbuf, inbuf, command)
     struct buffer *outbuf;
     struct buffer *inbuf;
     int command;
{
    while (1)
    {
	struct buffer_data *data;
	struct buffer_data *nldata;
	char *nl;
	int len;

	/* See if there is a newline in INBUF.  */
	nldata = NULL;
	nl = NULL;
	for (data = inbuf->data; data != NULL; data = data->next)
	{
	    nl = memchr (data->bufp, '\n', data->size);
	    if (nl != NULL)
	    {
		nldata = data;
		break;
	    }
	}

	if (nldata == NULL)
	{
	    /* There are no more lines in INBUF.  */
	    return;
	}

	/* Put in the command.  */
	buf_append_char (outbuf, command);
	buf_append_char (outbuf, ' ');

	if (inbuf->data != nldata)
	{
	    /*
	     * Simply move over all the buffers up to the one containing
	     * the newline.
	     */
	    for (data = inbuf->data; data->next != nldata; data = data->next)
		;
	    data->next = NULL;
	    buf_append_data (outbuf, inbuf->data, data);
	    inbuf->data = nldata;
	}

	/*
	 * If the newline is at the very end of the buffer, just move
	 * the buffer onto OUTBUF.  Otherwise we must copy the data.
	 */
	len = nl + 1 - nldata->bufp;
	if (len == nldata->size)
	{
	    inbuf->data = nldata->next;
	    if (inbuf->data == NULL)
		inbuf->last = NULL;

	    nldata->next = NULL;
	    buf_append_data (outbuf, nldata, nldata);
	}
	else
	{
	    buf_output (outbuf, nldata->bufp, len);
	    nldata->bufp += len;
	    nldata->size -= len;
	}
    }
}

/*
 * Copy counted data from one buffer to another.  The count is an
 * integer, host size, host byte order (it is only used across a
 * pipe).  If there is enough data, it should be moved over.  If there
 * is not enough data, it should remain on the original buffer.  A
 * negative count is a special case.  if one is seen, *SPECIAL is set
 * to the (negative) count value and no additional data is gathered
 * from the buffer; normally *SPECIAL is set to 0.  This function
 * returns the number of bytes it needs to see in order to actually
 * copy something over.
 */

int
buf_copy_counted (outbuf, inbuf, special)
     struct buffer *outbuf;
     struct buffer *inbuf;
     int *special;
{
    *special = 0;

    while (1)
    {
	struct buffer_data *data;
	int need;
	union
	{
	    char intbuf[sizeof (int)];
	    int i;
	} u;
	char *intp;
	int count;
	struct buffer_data *start;
	int startoff;
	struct buffer_data *stop;
	int stopwant;

	/* See if we have enough bytes to figure out the count.  */
	need = sizeof (int);
	intp = u.intbuf;
	for (data = inbuf->data; data != NULL; data = data->next)
	{
	    if (data->size >= need)
	    {
		memcpy (intp, data->bufp, need);
		break;
	    }
	    memcpy (intp, data->bufp, data->size);
	    intp += data->size;
	    need -= data->size;
	}
	if (data == NULL)
	{
	    /* We don't have enough bytes to form an integer.  */
	    return need;
	}

	count = u.i;
	start = data;
	startoff = need;

	if (count < 0)
	{
	    /* A negative COUNT is a special case meaning that we
               don't need any further information.  */
	    stop = start;
	    stopwant = 0;
	}
	else
	{
	    /*
	     * We have an integer in COUNT.  We have gotten all the
	     * data from INBUF in all buffers before START, and we
	     * have gotten STARTOFF bytes from START.  See if we have
	     * enough bytes remaining in INBUF.
	     */
	    need = count - (start->size - startoff);
	    if (need <= 0)
	    {
		stop = start;
		stopwant = count;
	    }
	    else
	    {
		for (data = start->next; data != NULL; data = data->next)
		{
		    if (need <= data->size)
			break;
		    need -= data->size;
		}
		if (data == NULL)
		{
		    /* We don't have enough bytes.  */
		    return need;
		}
		stop = data;
		stopwant = need;
	    }
	}

	/*
	 * We have enough bytes.  Free any buffers in INBUF before
	 * START, and remove STARTOFF bytes from START, so that we can
	 * forget about STARTOFF.
	 */
	start->bufp += startoff;
	start->size -= startoff;

	if (start->size == 0)
	    start = start->next;

	if (stop->size == stopwant)
	{
	    stop = stop->next;
	    stopwant = 0;
	}

	while (inbuf->data != start)
	{
	    data = inbuf->data;
	    inbuf->data = data->next;
	    data->next = free_buffer_data;
	    free_buffer_data = data;
	}

	/* If COUNT is negative, set *SPECIAL and get out now.  */
	if (count < 0)
	{
	    *special = count;
	    return 0;
	}

	/*
	 * We want to copy over the bytes from START through STOP.  We
	 * only want STOPWANT bytes from STOP.
	 */

	if (start != stop)
	{
	    /* Attach the buffers from START through STOP to OUTBUF.  */
	    for (data = start; data->next != stop; data = data->next)
		;
	    inbuf->data = stop;
	    data->next = NULL;
	    buf_append_data (outbuf, start, data);
	}

	if (stopwant > 0)
	{
	    buf_output (outbuf, stop->bufp, stopwant);
	    stop->bufp += stopwant;
	    stop->size -= stopwant;
	}
    }

    /*NOTREACHED*/
}

/* Shut down a buffer.  This returns 0 on success, or an errno code.  */

int
buf_shutdown (buf)
     struct buffer *buf;
{
    if (buf->shutdown)
	return (*buf->shutdown) (buf->closure);
    return 0;
}

/* The simplest type of buffer is one built on top of a stdio FILE.
   For simplicity, and because it is all that is required, we do not
   implement setting this type of buffer into nonblocking mode.  The
   closure field is just a FILE *.  */

static int stdio_buffer_input PROTO((void *, char *, int, int, int *));
static int stdio_buffer_output PROTO((void *, const char *, int, int *));
static int stdio_buffer_flush PROTO((void *));

/* Initialize a buffer built on a stdio FILE.  */

struct buffer
*stdio_buffer_initialize (fp, input, memory)
     FILE *fp;
     int input;
     void (*memory) PROTO((struct buffer *));
{
    return buf_initialize (input ? stdio_buffer_input : NULL,
			   input ? NULL : stdio_buffer_output,
			   input ? NULL : stdio_buffer_flush,
			   (int (*) PROTO((void *, int))) NULL,
			   (int (*) PROTO((void *))) NULL,
			   memory,
			   (void *) fp);
}

/* The buffer input function for a buffer built on a stdio FILE.  */

static int
stdio_buffer_input (closure, data, need, size, got)
     void *closure;
     char *data;
     int need;
     int size;
     int *got;
{
    FILE *fp = (FILE *) closure;
    int nbytes;

    /* Since stdio does its own buffering, we don't worry about
       getting more bytes than we need.  */

    if (need == 0 || need == 1)
    {
        int ch;

	ch = getc (fp);

	if (ch == EOF)
	{
	    if (feof (fp))
		return -1;
	    else if (errno == 0)
		return EIO;
	    else
		return errno;
	}

	*data = ch;
	*got = 1;
	return 0;
    }

    nbytes = fread (data, 1, need, fp);

    if (nbytes == 0)
    {
	*got = 0;
	if (feof (fp))
	    return -1;
	else if (errno == 0)
	    return EIO;
	else
	    return errno;
    }

    *got = nbytes;

    return 0;
}

/* The buffer output function for a buffer built on a stdio FILE.  */

static int
stdio_buffer_output (closure, data, have, wrote)
     void *closure;
     const char *data;
     int have;
     int *wrote;
{
    FILE *fp = (FILE *) closure;

    *wrote = 0;

    while (have > 0)
    {
	int nbytes;

	nbytes = fwrite (data, 1, have, fp);

	if (nbytes != have)
	{
	    if (errno == 0)
		return EIO;
	    else
		return errno;
	}

	*wrote += nbytes;
	have -= nbytes;
	data += nbytes;
    }

    return 0;
}

/* The buffer flush function for a buffer built on a stdio FILE.  */

static int
stdio_buffer_flush (closure)
     void *closure;
{
    FILE *fp = (FILE *) closure;

    if (fflush (fp) != 0)
    {
	if (errno == 0)
	    return EIO;
	else
	    return errno;
    }

    return 0;
}

#endif /* defined (SERVER_SUPPORT) || defined (CLIENT_SUPPORT) */
