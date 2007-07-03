/*
 * Copyright (C) 1984-2007  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Low level character input from the input file.
 * We use these special purpose routines which optimize moving
 * both forward and backward from the current read pointer.
 */

#include "less.h"
#if MSDOS_COMPILER==WIN32C
#include <errno.h>
#include <windows.h>
#endif

typedef POSITION BLOCKNUM;

public int ignore_eoi;

/*
 * Pool of buffers holding the most recently used blocks of the input file.
 * The buffer pool is kept as a doubly-linked circular list,
 * in order from most- to least-recently used.
 * The circular list is anchored by the file state "thisfile".
 */
#define	LBUFSIZE	8192
struct buf {
	struct buf *next, *prev;
	struct buf *hnext, *hprev;
	BLOCKNUM block;
	unsigned int datasize;
	unsigned char data[LBUFSIZE];
};

struct buflist {
	/* -- Following members must match struct buf */
	struct buf *buf_next, *buf_prev;
	struct buf *buf_hnext, *buf_hprev;
};

/*
 * The file state is maintained in a filestate structure.
 * A pointer to the filestate is kept in the ifile structure.
 */
#define	BUFHASH_SIZE	64
struct filestate {
	struct buf *buf_next, *buf_prev;
	struct buflist hashtbl[BUFHASH_SIZE];
	int file;
	int flags;
	POSITION fpos;
	int nbufs;
	BLOCKNUM block;
	unsigned int offset;
	POSITION fsize;
};

#define	ch_bufhead	thisfile->buf_next
#define	ch_buftail	thisfile->buf_prev
#define	ch_nbufs	thisfile->nbufs
#define	ch_block	thisfile->block
#define	ch_offset	thisfile->offset
#define	ch_fpos		thisfile->fpos
#define	ch_fsize	thisfile->fsize
#define	ch_flags	thisfile->flags
#define	ch_file		thisfile->file

#define	END_OF_CHAIN	((struct buf *)&thisfile->buf_next)
#define	END_OF_HCHAIN(h) ((struct buf *)&thisfile->hashtbl[h])
#define BUFHASH(blk)	((blk) & (BUFHASH_SIZE-1))

#define	FOR_BUFS_IN_CHAIN(h,bp) \
	for (bp = thisfile->hashtbl[h].buf_hnext;  \
	     bp != END_OF_HCHAIN(h);  bp = bp->hnext)

#define	HASH_RM(bp) \
	(bp)->hnext->hprev = (bp)->hprev; \
	(bp)->hprev->hnext = (bp)->hnext;

#define	HASH_INS(bp,h) \
	(bp)->hnext = thisfile->hashtbl[h].buf_hnext; \
	(bp)->hprev = END_OF_HCHAIN(h); \
	thisfile->hashtbl[h].buf_hnext->hprev = (bp); \
	thisfile->hashtbl[h].buf_hnext = (bp);

static struct filestate *thisfile;
static int ch_ungotchar = -1;
static int maxbufs = -1;

extern int autobuf;
extern int sigs;
extern int secure;
extern constant char helpdata[];
extern constant int size_helpdata;
extern IFILE curr_ifile;
#if LOGFILE
extern int logfile;
extern char *namelogfile;
#endif

static int ch_addbuf();


/*
 * Get the character pointed to by the read pointer.
 * ch_get() is a macro which is more efficient to call
 * than fch_get (the function), in the usual case 
 * that the block desired is at the head of the chain.
 */
#define	ch_get()   ((ch_block == ch_bufhead->block && \
		     ch_offset < ch_bufhead->datasize) ? \
			ch_bufhead->data[ch_offset] : fch_get())
	int
fch_get()
{
	register struct buf *bp;
	register int n;
	register int slept;
	register int h;
	POSITION pos;
	POSITION len;

	slept = FALSE;

	/*
	 * Look for a buffer holding the desired block.
	 */
	h = BUFHASH(ch_block);
	FOR_BUFS_IN_CHAIN(h, bp)
	{
		if (bp->block == ch_block)
		{
			if (ch_offset >= bp->datasize)
				/*
				 * Need more data in this buffer.
				 */
				goto read_more;
			goto found;
		}
	}
	/*
	 * Block is not in a buffer.  
	 * Take the least recently used buffer 
	 * and read the desired block into it.
	 * If the LRU buffer has data in it, 
	 * then maybe allocate a new buffer.
	 */
	if (ch_buftail == END_OF_CHAIN || ch_buftail->block != -1)
	{
		/*
		 * There is no empty buffer to use.
		 * Allocate a new buffer if:
		 * 1. We can't seek on this file and -b is not in effect; or
		 * 2. We haven't allocated the max buffers for this file yet.
		 */
		if ((autobuf && !(ch_flags & CH_CANSEEK)) ||
		    (maxbufs < 0 || ch_nbufs < maxbufs))
			if (ch_addbuf())
				/*
				 * Allocation failed: turn off autobuf.
				 */
				autobuf = OPT_OFF;
	}
	bp = ch_buftail;
	HASH_RM(bp); /* Remove from old hash chain. */
	bp->block = ch_block;
	bp->datasize = 0;
	HASH_INS(bp, h); /* Insert into new hash chain. */

    read_more:
	pos = (ch_block * LBUFSIZE) + bp->datasize;
	if ((len = ch_length()) != NULL_POSITION && pos >= len)
		/*
		 * At end of file.
		 */
		return (EOI);

	if (pos != ch_fpos)
	{
		/*
		 * Not at the correct position: must seek.
		 * If input is a pipe, we're in trouble (can't seek on a pipe).
		 * Some data has been lost: just return "?".
		 */
		if (!(ch_flags & CH_CANSEEK))
			return ('?');
		if (lseek(ch_file, (off_t)pos, 0) == BAD_LSEEK)
		{
 			error("seek error", NULL_PARG);
			clear_eol();
			return (EOI);
 		}
 		ch_fpos = pos;
 	}

	/*
	 * Read the block.
	 * If we read less than a full block, that's ok.
	 * We use partial block and pick up the rest next time.
	 */
	if (ch_ungotchar != -1)
	{
		bp->data[bp->datasize] = ch_ungotchar;
		n = 1;
		ch_ungotchar = -1;
	} else if (ch_flags & CH_HELPFILE)
	{
		bp->data[bp->datasize] = helpdata[ch_fpos];
		n = 1;
	} else
	{
		n = iread(ch_file, &bp->data[bp->datasize], 
			(unsigned int)(LBUFSIZE - bp->datasize));
	}

	if (n == READ_INTR)
		return (EOI);
	if (n < 0)
	{
#if MSDOS_COMPILER==WIN32C
		if (errno != EPIPE)
#endif
		{
			error("read error", NULL_PARG);
			clear_eol();
		}
		n = 0;
	}

#if LOGFILE
	/*
	 * If we have a log file, write the new data to it.
	 */
	if (!secure && logfile >= 0 && n > 0)
		write(logfile, (char *) &bp->data[bp->datasize], n);
#endif

	ch_fpos += n;
	bp->datasize += n;

	/*
	 * If we have read to end of file, set ch_fsize to indicate
	 * the position of the end of file.
	 */
	if (n == 0)
	{
		ch_fsize = pos;
		if (ignore_eoi)
		{
			/*
			 * We are ignoring EOF.
			 * Wait a while, then try again.
			 */
			if (!slept)
			{
				PARG parg;
				parg.p_string = wait_message();
				ierror("%s", &parg);
			}
#if !MSDOS_COMPILER
	 		sleep(1);
#else
#if MSDOS_COMPILER==WIN32C
			Sleep(1000);
#endif
#endif
			slept = TRUE;
		}
		if (sigs)
			return (EOI);
	}

    found:
	if (ch_bufhead != bp)
	{
		/*
		 * Move the buffer to the head of the buffer chain.
		 * This orders the buffer chain, most- to least-recently used.
		 */
		bp->next->prev = bp->prev;
		bp->prev->next = bp->next;
		bp->next = ch_bufhead;
		bp->prev = END_OF_CHAIN;
		ch_bufhead->prev = bp;
		ch_bufhead = bp;

		/*
		 * Move to head of hash chain too.
		 */
		HASH_RM(bp);
		HASH_INS(bp, h);
	}

	if (ch_offset >= bp->datasize)
		/*
		 * After all that, we still don't have enough data.
		 * Go back and try again.
		 */
		goto read_more;

	return (bp->data[ch_offset]);
}

/*
 * ch_ungetchar is a rather kludgy and limited way to push 
 * a single char onto an input file descriptor.
 */
	public void
ch_ungetchar(c)
	int c;
{
	if (c != -1 && ch_ungotchar != -1)
		error("ch_ungetchar overrun", NULL_PARG);
	ch_ungotchar = c;
}

#if LOGFILE
/*
 * Close the logfile.
 * If we haven't read all of standard input into it, do that now.
 */
	public void
end_logfile()
{
	static int tried = FALSE;

	if (logfile < 0)
		return;
	if (!tried && ch_fsize == NULL_POSITION)
	{
		tried = TRUE;
		ierror("Finishing logfile", NULL_PARG);
		while (ch_forw_get() != EOI)
			if (ABORT_SIGS())
				break;
	}
	close(logfile);
	logfile = -1;
	namelogfile = NULL;
}

/*
 * Start a log file AFTER less has already been running.
 * Invoked from the - command; see toggle_option().
 * Write all the existing buffered data to the log file.
 */
	public void
sync_logfile()
{
	register struct buf *bp;
	int warned = FALSE;
	BLOCKNUM block;
	BLOCKNUM nblocks;

	nblocks = (ch_fpos + LBUFSIZE - 1) / LBUFSIZE;
	for (block = 0;  block < nblocks;  block++)
	{
		for (bp = ch_bufhead;  ;  bp = bp->next)
		{
			if (bp == END_OF_CHAIN)
			{
				if (!warned)
				{
					error("Warning: log file is incomplete",
						NULL_PARG);
					warned = TRUE;
				}
				break;
			}
			if (bp->block == block)
			{
				write(logfile, (char *) bp->data, bp->datasize);
				break;
			}
		}
	}
}

#endif

/*
 * Determine if a specific block is currently in one of the buffers.
 */
	static int
buffered(block)
	BLOCKNUM block;
{
	register struct buf *bp;
	register int h;

	h = BUFHASH(block);
	FOR_BUFS_IN_CHAIN(h, bp)
	{
		if (bp->block == block)
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Seek to a specified position in the file.
 * Return 0 if successful, non-zero if can't seek there.
 */
	public int
ch_seek(pos)
	register POSITION pos;
{
	BLOCKNUM new_block;
	POSITION len;

	len = ch_length();
	if (pos < ch_zero() || (len != NULL_POSITION && pos > len))
		return (1);

	new_block = pos / LBUFSIZE;
	if (!(ch_flags & CH_CANSEEK) && pos != ch_fpos && !buffered(new_block))
	{
		if (ch_fpos > pos)
			return (1);
		while (ch_fpos < pos)
		{
			if (ch_forw_get() == EOI)
				return (1);
			if (ABORT_SIGS())
				return (1);
		}
		return (0);
	}
	/*
	 * Set read pointer.
	 */
	ch_block = new_block;
	ch_offset = pos % LBUFSIZE;
	return (0);
}

/*
 * Seek to the end of the file.
 */
	public int
ch_end_seek()
{
	POSITION len;

	if (ch_flags & CH_CANSEEK)
		ch_fsize = filesize(ch_file);

	len = ch_length();
	if (len != NULL_POSITION)
		return (ch_seek(len));

	/*
	 * Do it the slow way: read till end of data.
	 */
	while (ch_forw_get() != EOI)
		if (ABORT_SIGS())
			return (1);
	return (0);
}

/*
 * Seek to the beginning of the file, or as close to it as we can get.
 * We may not be able to seek there if input is a pipe and the
 * beginning of the pipe is no longer buffered.
 */
	public int
ch_beg_seek()
{
	register struct buf *bp, *firstbp;

	/*
	 * Try a plain ch_seek first.
	 */
	if (ch_seek(ch_zero()) == 0)
		return (0);

	/*
	 * Can't get to position 0.
	 * Look thru the buffers for the one closest to position 0.
	 */
	firstbp = bp = ch_bufhead;
	if (bp == END_OF_CHAIN)
		return (1);
	while ((bp = bp->next) != END_OF_CHAIN)
		if (bp->block < firstbp->block)
			firstbp = bp;
	ch_block = firstbp->block;
	ch_offset = 0;
	return (0);
}

/*
 * Return the length of the file, if known.
 */
	public POSITION
ch_length()
{
	if (ignore_eoi)
		return (NULL_POSITION);
	if (ch_flags & CH_HELPFILE)
		return (size_helpdata);
	return (ch_fsize);
}

/*
 * Return the current position in the file.
 */
	public POSITION
ch_tell()
{
	return (ch_block * LBUFSIZE) + ch_offset;
}

/*
 * Get the current char and post-increment the read pointer.
 */
	public int
ch_forw_get()
{
	register int c;

	c = ch_get();
	if (c == EOI)
		return (EOI);
	if (ch_offset < LBUFSIZE-1)
		ch_offset++;
	else
	{
		ch_block ++;
		ch_offset = 0;
	}
	return (c);
}

/*
 * Pre-decrement the read pointer and get the new current char.
 */
	public int
ch_back_get()
{
	if (ch_offset > 0)
		ch_offset --;
	else
	{
		if (ch_block <= 0)
			return (EOI);
		if (!(ch_flags & CH_CANSEEK) && !buffered(ch_block-1))
			return (EOI);
		ch_block--;
		ch_offset = LBUFSIZE-1;
	}
	return (ch_get());
}

/*
 * Set max amount of buffer space.
 * bufspace is in units of 1024 bytes.  -1 mean no limit.
 */
	public void
ch_setbufspace(bufspace)
	int bufspace;
{
	if (bufspace < 0)
		maxbufs = -1;
	else
	{
		maxbufs = ((bufspace * 1024) + LBUFSIZE-1) / LBUFSIZE;
		if (maxbufs < 1)
			maxbufs = 1;
	}
}

/*
 * Flush (discard) any saved file state, including buffer contents.
 */
	public void
ch_flush()
{
	register struct buf *bp;

	if (!(ch_flags & CH_CANSEEK))
	{
		/*
		 * If input is a pipe, we don't flush buffer contents,
		 * since the contents can't be recovered.
		 */
		ch_fsize = NULL_POSITION;
		return;
	}

	/*
	 * Initialize all the buffers.
	 */
	for (bp = ch_bufhead;  bp != END_OF_CHAIN;  bp = bp->next)
		bp->block = -1;

	/*
	 * Figure out the size of the file, if we can.
	 */
	ch_fsize = filesize(ch_file);

	/*
	 * Seek to a known position: the beginning of the file.
	 */
	ch_fpos = 0;
	ch_block = 0; /* ch_fpos / LBUFSIZE; */
	ch_offset = 0; /* ch_fpos % LBUFSIZE; */

#if 1
	/*
	 * This is a kludge to workaround a Linux kernel bug: files in
	 * /proc have a size of 0 according to fstat() but have readable 
	 * data.  They are sometimes, but not always, seekable.
	 * Force them to be non-seekable here.
	 */
	if (ch_fsize == 0)
	{
		ch_fsize = NULL_POSITION;
		ch_flags &= ~CH_CANSEEK;
	}
#endif

	if (lseek(ch_file, (off_t)0, 0) == BAD_LSEEK)
	{
		/*
		 * Warning only; even if the seek fails for some reason,
		 * there's a good chance we're at the beginning anyway.
		 * {{ I think this is bogus reasoning. }}
		 */
		error("seek error to 0", NULL_PARG);
	}
}

/*
 * Allocate a new buffer.
 * The buffer is added to the tail of the buffer chain.
 */
	static int
ch_addbuf()
{
	register struct buf *bp;

	/*
	 * Allocate and initialize a new buffer and link it 
	 * onto the tail of the buffer list.
	 */
	bp = (struct buf *) calloc(1, sizeof(struct buf));
	if (bp == NULL)
		return (1);
	ch_nbufs++;
	bp->block = -1;
	bp->next = END_OF_CHAIN;
	bp->prev = ch_buftail;
	ch_buftail->next = bp;
	ch_buftail = bp;
	HASH_INS(bp, 0);
	return (0);
}

/*
 *
 */
	static void
init_hashtbl()
{
	register int h;

	for (h = 0;  h < BUFHASH_SIZE;  h++)
	{
		thisfile->hashtbl[h].buf_hnext = END_OF_HCHAIN(h);
		thisfile->hashtbl[h].buf_hprev = END_OF_HCHAIN(h);
	}
}

/*
 * Delete all buffers for this file.
 */
	static void
ch_delbufs()
{
	register struct buf *bp;

	while (ch_bufhead != END_OF_CHAIN)
	{
		bp = ch_bufhead;
		bp->next->prev = bp->prev;;
		bp->prev->next = bp->next;
		free(bp);
	}
	ch_nbufs = 0;
	init_hashtbl();
}

/*
 * Is it possible to seek on a file descriptor?
 */
	public int
seekable(f)
	int f;
{
#if MSDOS_COMPILER
	extern int fd0;
	if (f == fd0 && !isatty(fd0))
	{
		/*
		 * In MS-DOS, pipes are seekable.  Check for
		 * standard input, and pretend it is not seekable.
		 */
		return (0);
	}
#endif
	return (lseek(f, (off_t)1, 0) != BAD_LSEEK);
}

/*
 * Initialize file state for a new file.
 */
	public void
ch_init(f, flags)
	int f;
	int flags;
{
	/*
	 * See if we already have a filestate for this file.
	 */
	thisfile = (struct filestate *) get_filestate(curr_ifile);
	if (thisfile == NULL)
	{
		/*
		 * Allocate and initialize a new filestate.
		 */
		thisfile = (struct filestate *) 
				calloc(1, sizeof(struct filestate));
		thisfile->buf_next = thisfile->buf_prev = END_OF_CHAIN;
		thisfile->nbufs = 0;
		thisfile->flags = 0;
		thisfile->fpos = 0;
		thisfile->block = 0;
		thisfile->offset = 0;
		thisfile->file = -1;
		thisfile->fsize = NULL_POSITION;
		ch_flags = flags;
		init_hashtbl();
		/*
		 * Try to seek; set CH_CANSEEK if it works.
		 */
		if ((flags & CH_CANSEEK) && !seekable(f))
			ch_flags &= ~CH_CANSEEK;
		set_filestate(curr_ifile, (void *) thisfile);
	}
	if (thisfile->file == -1)
		thisfile->file = f;
	ch_flush();
}

/*
 * Close a filestate.
 */
	public void
ch_close()
{
	int keepstate = FALSE;

	if (ch_flags & (CH_CANSEEK|CH_POPENED|CH_HELPFILE))
	{
		/*
		 * We can seek or re-open, so we don't need to keep buffers.
		 */
		ch_delbufs();
	} else
		keepstate = TRUE;
	if (!(ch_flags & CH_KEEPOPEN))
	{
		/*
		 * We don't need to keep the file descriptor open
		 * (because we can re-open it.)
		 * But don't really close it if it was opened via popen(),
		 * because pclose() wants to close it.
		 */
		if (!(ch_flags & (CH_POPENED|CH_HELPFILE)))
			close(ch_file);
		ch_file = -1;
	} else
		keepstate = TRUE;
	if (!keepstate)
	{
		/*
		 * We don't even need to keep the filestate structure.
		 */
		free(thisfile);
		thisfile = NULL;
		set_filestate(curr_ifile, (void *) NULL);
	}
}

/*
 * Return ch_flags for the current file.
 */
	public int
ch_getflags()
{
	return (ch_flags);
}

#if 0
	public void
ch_dump(struct filestate *fs)
{
	struct buf *bp;
	unsigned char *s;

	if (fs == NULL)
	{
		printf(" --no filestate\n");
		return;
	}
	printf(" file %d, flags %x, fpos %x, fsize %x, blk/off %x/%x\n",
		fs->file, fs->flags, fs->fpos, 
		fs->fsize, fs->block, fs->offset);
	printf(" %d bufs:\n", fs->nbufs);
	for (bp = fs->buf_next; bp != (struct buf *)fs;  bp = bp->next)
	{
		printf("%x: blk %x, size %x \"",
			bp, bp->block, bp->datasize);
		for (s = bp->data;  s < bp->data + 30;  s++)
			if (*s >= ' ' && *s < 0x7F)
				printf("%c", *s);
			else
				printf(".");
		printf("\"\n");
	}
}
#endif
