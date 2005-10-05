/* $FreeBSD$ */

/* util.c - Several utility routines for cpio.
   Copyright (C) 1990, 1991, 1992, 2001, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <system.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "cpiohdr.h"
#include "dstring.h"
#include "extern.h"
#include <rmt.h>

#include <sys/ioctl.h>

#ifdef HAVE_SYS_MTIO_H
#ifdef HAVE_SYS_IO_TRIOCTL_H
#include <sys/io/trioctl.h>
#endif
#include <sys/mtio.h>
#endif

#if !HAVE_DECL_ERRNO
extern int errno;
#endif

/* Write `output_size' bytes of `output_buffer' to file
   descriptor OUT_DES and reset `output_size' and `out_buff'.  */

void
tape_empty_output_buffer (int out_des)
{
  int bytes_written;

#ifdef BROKEN_LONG_TAPE_DRIVER
  static long output_bytes_before_lseek = 0;

  /* Some tape drivers seem to have a signed internal seek pointer and
     they lose if it overflows and becomes negative (e.g. when writing 
     tapes > 2Gb).  Doing an lseek (des, 0, SEEK_SET) seems to reset the 
     seek pointer and prevent it from overflowing.  */
  if (output_is_special
     && ( (output_bytes_before_lseek += output_size) >= 1073741824L) )
    {
      lseek(out_des, 0L, SEEK_SET);
      output_bytes_before_lseek = 0;
    }
#endif

  bytes_written = rmtwrite (out_des, output_buffer, output_size);
  if (bytes_written != output_size)
    {
      int rest_bytes_written;
      int rest_output_size;

      if (output_is_special
	  && (bytes_written >= 0
	      || (bytes_written < 0
		  && (errno == ENOSPC || errno == EIO || errno == ENXIO))))
	{
	  get_next_reel (out_des);
	  if (bytes_written > 0)
	    rest_output_size = output_size - bytes_written;
	  else
	    rest_output_size = output_size;
	  rest_bytes_written = rmtwrite (out_des, output_buffer,
					 rest_output_size);
	  if (rest_bytes_written != rest_output_size)
	    error (1, errno, _("write error"));
	}
      else
	error (1, errno, _("write error"));
    }
  output_bytes += output_size;
  out_buff = output_buffer;
  output_size = 0;
}

/* Write `output_size' bytes of `output_buffer' to file
   descriptor OUT_DES and reset `output_size' and `out_buff'.
   If `swapping_halfwords' or `swapping_bytes' is set,
   do the appropriate swapping first.  Our callers have
   to make sure to only set these flags if `output_size' 
   is appropriate (a multiple of 4 for `swapping_halfwords',
   2 for `swapping_bytes').  The fact that DISK_IO_BLOCK_SIZE
   must always be a multiple of 4 helps us (and our callers)
   insure this.  */

void
disk_empty_output_buffer (int out_des)
{
  int bytes_written;

  if (swapping_halfwords || swapping_bytes)
    {
      if (swapping_halfwords)
	{
	  int complete_words;
	  complete_words = output_size / 4;
	  swahw_array (output_buffer, complete_words);
	  if (swapping_bytes)
	    swab_array (output_buffer, 2 * complete_words);
	}
      else
	{
	  int complete_halfwords;
	  complete_halfwords = output_size /2;
	  swab_array (output_buffer, complete_halfwords);
	}
    }

  if (sparse_flag)
    bytes_written = sparse_write (out_des, output_buffer, output_size);
  else
    bytes_written = write (out_des, output_buffer, output_size);

  if (bytes_written != output_size)
    {
      error (1, errno, _("write error"));
    }
  output_bytes += output_size;
  out_buff = output_buffer;
  output_size = 0;
}

/* Exchange the halfwords of each element of the array of COUNT longs
   starting at PTR.  PTR does not have to be aligned at a word
   boundary.  */

void
swahw_array (char *ptr, int count)
{
  char tmp;

  for (; count > 0; --count)
    {
      tmp = *ptr;
      *ptr = *(ptr + 2);
      *(ptr + 2) = tmp;
      ++ptr;
      tmp = *ptr;
      *ptr = *(ptr + 2);
      *(ptr + 2) = tmp;
      ptr += 3;
    }
}

/* Read at most NUM_BYTES or `io_block_size' bytes, whichever is smaller,
   into the start of `input_buffer' from file descriptor IN_DES.
   Set `input_size' to the number of bytes read and reset `in_buff'.
   Exit with an error if end of file is reached.  */

#ifdef BROKEN_LONG_TAPE_DRIVER
static long input_bytes_before_lseek = 0;
#endif

static void
tape_fill_input_buffer (int in_des, int num_bytes)
{
#ifdef BROKEN_LONG_TAPE_DRIVER
  /* Some tape drivers seem to have a signed internal seek pointer and
     they lose if it overflows and becomes negative (e.g. when writing 
     tapes > 4Gb).  Doing an lseek (des, 0, SEEK_SET) seems to reset the 
     seek pointer and prevent it from overflowing.  */
  if (input_is_special
      && ( (input_bytes_before_lseek += num_bytes) >= 1073741824L) )
    {
      lseek(in_des, 0L, SEEK_SET);
      input_bytes_before_lseek = 0;
    }
#endif
  in_buff = input_buffer;
  num_bytes = (num_bytes < io_block_size) ? num_bytes : io_block_size;
  input_size = rmtread (in_des, input_buffer, num_bytes);
  if (input_size == 0 && input_is_special)
    {
      get_next_reel (in_des);
      input_size = rmtread (in_des, input_buffer, num_bytes);
    }
  if (input_size < 0)
    error (1, errno, _("read error"));
  if (input_size == 0)
    {
      error (0, 0, _("premature end of file"));
      exit (1);
    }
  input_bytes += input_size;
}

/* Read at most NUM_BYTES or `DISK_IO_BLOCK_SIZE' bytes, whichever is smaller,
   into the start of `input_buffer' from file descriptor IN_DES.
   Set `input_size' to the number of bytes read and reset `in_buff'.
   Exit with an error if end of file is reached.  */

static int
disk_fill_input_buffer (int in_des, int num_bytes)
{
  in_buff = input_buffer;
  num_bytes = (num_bytes < DISK_IO_BLOCK_SIZE) ? num_bytes : DISK_IO_BLOCK_SIZE;
  input_size = read (in_des, input_buffer, num_bytes);
  if (input_size < 0) 
    {
      input_size = 0;
      return (-1);
    }
  else if (input_size == 0)
    return (1);
  input_bytes += input_size;
  return (0);
}

/* Copy NUM_BYTES of buffer IN_BUF to `out_buff', which may be partly full.
   When `out_buff' fills up, flush it to file descriptor OUT_DES.  */

void
tape_buffered_write (char *in_buf, int out_des, long num_bytes)
{
  register long bytes_left = num_bytes;	/* Bytes needing to be copied.  */
  register long space_left;	/* Room left in output buffer.  */

  while (bytes_left > 0)
    {
      space_left = io_block_size - output_size;
      if (space_left == 0)
	tape_empty_output_buffer (out_des);
      else
	{
	  if (bytes_left < space_left)
	    space_left = bytes_left;
	  bcopy (in_buf, out_buff, (unsigned) space_left);
	  out_buff += space_left;
	  output_size += space_left;
	  in_buf += space_left;
	  bytes_left -= space_left;
	}
    }
}

/* Copy NUM_BYTES of buffer IN_BUF to `out_buff', which may be partly full.
   When `out_buff' fills up, flush it to file descriptor OUT_DES.  */

void
disk_buffered_write (char *in_buf, int out_des, long num_bytes)
{
  register long bytes_left = num_bytes;	/* Bytes needing to be copied.  */
  register long space_left;	/* Room left in output buffer.  */

  while (bytes_left > 0)
    {
      space_left = DISK_IO_BLOCK_SIZE - output_size;
      if (space_left == 0)
	disk_empty_output_buffer (out_des);
      else
	{
	  if (bytes_left < space_left)
	    space_left = bytes_left;
	  bcopy (in_buf, out_buff, (unsigned) space_left);
	  out_buff += space_left;
	  output_size += space_left;
	  in_buf += space_left;
	  bytes_left -= space_left;
	}
    }
}

/* Copy NUM_BYTES of buffer `in_buff' into IN_BUF.
   `in_buff' may be partly full.
   When `in_buff' is exhausted, refill it from file descriptor IN_DES.  */

void
tape_buffered_read (char *in_buf, int in_des, long num_bytes)
{
  register long bytes_left = num_bytes;	/* Bytes needing to be copied.  */
  register long space_left;	/* Bytes to copy from input buffer.  */

  while (bytes_left > 0)
    {
      if (input_size == 0)
	tape_fill_input_buffer (in_des, io_block_size);
      if (bytes_left < input_size)
	space_left = bytes_left;
      else
	space_left = input_size;
      bcopy (in_buff, in_buf, (unsigned) space_left);
      in_buff += space_left;
      in_buf += space_left;
      input_size -= space_left;
      bytes_left -= space_left;
    }
}

/* Copy the the next NUM_BYTES bytes of `input_buffer' into PEEK_BUF.
   If NUM_BYTES bytes are not available, read the next `io_block_size' bytes
   into the end of `input_buffer' and update `input_size'.

   Return the number of bytes copied into PEEK_BUF.
   If the number of bytes returned is less than NUM_BYTES,
   then EOF has been reached.  */

int
tape_buffered_peek (char *peek_buf, int in_des, int num_bytes)
{
  long tmp_input_size;
  long got_bytes;
  char *append_buf;

#ifdef BROKEN_LONG_TAPE_DRIVER
  /* Some tape drivers seem to have a signed internal seek pointer and
     they lose if it overflows and becomes negative (e.g. when writing 
     tapes > 4Gb).  Doing an lseek (des, 0, SEEK_SET) seems to reset the 
     seek pointer and prevent it from overflowing.  */
  if (input_is_special
      && ( (input_bytes_before_lseek += num_bytes) >= 1073741824L) )
    {
      lseek(in_des, 0L, SEEK_SET);
      input_bytes_before_lseek = 0;
    }
#endif

  while (input_size < num_bytes)
    {
      append_buf = in_buff + input_size;
      if ( (append_buf - input_buffer) >= input_buffer_size)
	{
	  /* We can keep up to 2 "blocks" (either the physical block size
	     or 512 bytes(the size of a tar record), which ever is
	     larger) in the input buffer when we are peeking.  We
	     assume that our caller will never be interested in peeking
	     ahead at more than 512 bytes, so we know that by the time
	     we need a 3rd "block" in the buffer we can throw away the
	     first block to make room.  */
	  int half;
	  half = input_buffer_size / 2;
	  bcopy (input_buffer + half, input_buffer, half);
	  in_buff = in_buff - half;
	  append_buf = append_buf - half;
	}
      tmp_input_size = rmtread (in_des, append_buf, io_block_size);
      if (tmp_input_size == 0)
	{
	  if (input_is_special)
	    {
	      get_next_reel (in_des);
	      tmp_input_size = rmtread (in_des, append_buf, io_block_size);
	    }
	  else
	    break;
	}
      if (tmp_input_size < 0)
	error (1, errno, _("read error"));
      input_bytes += tmp_input_size;
      input_size += tmp_input_size;
    }
  if (num_bytes <= input_size)
    got_bytes = num_bytes;
  else
    got_bytes = input_size;
  bcopy (in_buff, peek_buf, (unsigned) got_bytes);
  return got_bytes;
}

/* Skip the next NUM_BYTES bytes of file descriptor IN_DES.  */

void
tape_toss_input (int in_des, long num_bytes)
{
  register long bytes_left = num_bytes;	/* Bytes needing to be copied.  */
  register long space_left;	/* Bytes to copy from input buffer.  */

  while (bytes_left > 0)
    {
      if (input_size == 0)
	tape_fill_input_buffer (in_des, io_block_size);
      if (bytes_left < input_size)
	space_left = bytes_left;
      else
	space_left = input_size;

      if (crc_i_flag && only_verify_crc_flag)
	{
 	  int k;
	  for (k = 0; k < space_left; ++k)
	    crc += in_buff[k] & 0xff;
	}

      in_buff += space_left;
      input_size -= space_left;
      bytes_left -= space_left;
    }
}

static void
write_nuls_to_file (long num_bytes, int out_des, 
                    void (*writer) (char *in_buf, int out_des, long num_bytes))
{
  long	blocks;
  long	extra_bytes;
  long	i;

  blocks = num_bytes / 512;
  extra_bytes = num_bytes % 512;
  for (i = 0; i < blocks; ++i)
    writer (zeros_512, out_des, 512);
  if (extra_bytes)
    writer (zeros_512, out_des, extra_bytes);
}

/* Copy a file using the input and output buffers, which may start out
   partly full.  After the copy, the files are not closed nor the last
   block flushed to output, and the input buffer may still be partly
   full.  If `crc_i_flag' is set, add each byte to `crc'.
   IN_DES is the file descriptor for input;
   OUT_DES is the file descriptor for output;
   NUM_BYTES is the number of bytes to copy.  */

void
copy_files_tape_to_disk (int in_des, int out_des, long num_bytes)
{
  long size;
  long k;

  while (num_bytes > 0)
    {
      if (input_size == 0)
	tape_fill_input_buffer (in_des, io_block_size);
      size = (input_size < num_bytes) ? input_size : num_bytes;
      if (crc_i_flag)
	{
	  for (k = 0; k < size; ++k)
	    crc += in_buff[k] & 0xff;
	}
      disk_buffered_write (in_buff, out_des, size);
      num_bytes -= size;
      input_size -= size;
      in_buff += size;
    }
}
/* Copy a file using the input and output buffers, which may start out
   partly full.  After the copy, the files are not closed nor the last
   block flushed to output, and the input buffer may still be partly
   full.  If `crc_i_flag' is set, add each byte to `crc'.
   IN_DES is the file descriptor for input;
   OUT_DES is the file descriptor for output;
   NUM_BYTES is the number of bytes to copy.  */

void
copy_files_disk_to_tape (int in_des, int out_des, long num_bytes,
			 char *filename)
{
  long size;
  long k;
  int rc;
  long original_num_bytes;

  original_num_bytes = num_bytes;

  while (num_bytes > 0)
    {
      if (input_size == 0)
	if (rc = disk_fill_input_buffer (in_des,
	  num_bytes < DISK_IO_BLOCK_SIZE ?
	  num_bytes : DISK_IO_BLOCK_SIZE))
	  {
	    if (rc > 0)
	      error (0, 0, _("File %s shrunk by %ld bytes, padding with zeros"),
				filename, num_bytes);
	    else
	      error (0, 0, _("Read error at byte %ld in file %s, padding with zeros"),
			original_num_bytes - num_bytes, filename);
	    write_nuls_to_file (num_bytes, out_des, tape_buffered_write);
	    break;
	  }
      size = (input_size < num_bytes) ? input_size : num_bytes;
      if (crc_i_flag)
	{
	  for (k = 0; k < size; ++k)
	    crc += in_buff[k] & 0xff;
	}
      tape_buffered_write (in_buff, out_des, size);
      num_bytes -= size;
      input_size -= size;
      in_buff += size;
    }
}
/* Copy a file using the input and output buffers, which may start out
   partly full.  After the copy, the files are not closed nor the last
   block flushed to output, and the input buffer may still be partly
   full.  If `crc_i_flag' is set, add each byte to `crc'.
   IN_DES is the file descriptor for input;
   OUT_DES is the file descriptor for output;
   NUM_BYTES is the number of bytes to copy.  */

void
copy_files_disk_to_disk (int in_des, int out_des, long num_bytes,
			 char *filename)
{
  long size;
  long k;
  long original_num_bytes;
  int rc;

  original_num_bytes = num_bytes;
  while (num_bytes > 0)
    {
      if (input_size == 0)
	if (rc = disk_fill_input_buffer (in_des, num_bytes))
	  {
	    if (rc > 0)
	      error (0, 0, _("File %s shrunk by %ld bytes, padding with zeros"),
				filename, num_bytes);
	    else
	      error (0, 0, _("Read error at byte %ld in file %s, padding with zeros"),
			original_num_bytes - num_bytes, filename);
	    write_nuls_to_file (num_bytes, out_des, disk_buffered_write);
	    break;
	  }
      size = (input_size < num_bytes) ? input_size : num_bytes;
      if (crc_i_flag)
	{
	  for (k = 0; k < size; ++k)
	    crc += in_buff[k] & 0xff;
	}
      disk_buffered_write (in_buff, out_des, size);
      num_bytes -= size;
      input_size -= size;
      in_buff += size;
    }
}

/* Warn if file changed while it was being copied.  */

void
warn_if_file_changed (char *file_name, unsigned long old_file_size,
		      unsigned long old_file_mtime)
{
  struct stat new_file_stat;
  if ((*xstat) (file_name, &new_file_stat) < 0)
    {
      error (0, errno, "%s", file_name);
      return;
    }

  /* Only check growth, shrinkage detected in copy_files_disk_to_{disk,tape}()
   */
  if (new_file_stat.st_size > old_file_size)
    error (0, 0, _("File %s grew, %ld new bytes not copied"),
	   file_name, (long)(new_file_stat.st_size - old_file_size));

  else if (new_file_stat.st_mtime != old_file_mtime)
    error (0, 0, _("File %s was modified while being copied"), file_name);
}

/* Create all directories up to but not including the last part of NAME.
   Do not destroy any nondirectories while creating directories.  */

void
create_all_directories (char *name)
{
  char *dir;
  int   mode;
#ifdef HPUX_CDF
  int   cdf;
#endif

  dir = dir_name (name);
  mode = 0700;
#ifdef HPUX_CDF
  cdf = islastparentcdf (name);
  if (cdf)
    {
      dir [strlen (dir) - 1] = '\0';	/* remove final + */
      mode = 04700;
    }
  
#endif
  
  if (dir == NULL)
    error (2, 0, _("virtual memory exhausted"));

  if (dir[0] != '.' || dir[1] != '\0')
    make_path (dir, mode, 0700, -1, -1, (char *) NULL);

  free (dir);
}

/* Prepare to append to an archive.  We have been in
   process_copy_in, keeping track of the position where
   the last header started in `last_header_start'.  Now we
   have the starting position of the last header (the TRAILER!!!
   header, or blank record for tar archives) and we want to start
   writing (appending) over the last header.  The last header may
   be in the middle of a block, so to keep the buffering in sync
   we lseek back to the start of the block, read everything up
   to but not including the last header, lseek back to the start
   of the block, and then do a copy_buf_out of what we read.
   Actually, we probably don't have to worry so much about keeping the
   buffering perfect since you can only append to archives that
   are disk files.  */

void
prepare_append (int out_file_des)
{
  int start_of_header;
  int start_of_block;
  int useful_bytes_in_block;
  char *tmp_buf;

  start_of_header = last_header_start;
  /* Figure out how many bytes we will rewrite, and where they start.  */
  useful_bytes_in_block = start_of_header % io_block_size;
  start_of_block = start_of_header - useful_bytes_in_block;

  if (lseek (out_file_des, start_of_block, SEEK_SET) < 0)
    error (1, errno, _("cannot seek on output"));
  if (useful_bytes_in_block > 0)
    {
      tmp_buf = (char *) xmalloc (useful_bytes_in_block);
      read (out_file_des, tmp_buf, useful_bytes_in_block);
      if (lseek (out_file_des, start_of_block, SEEK_SET) < 0)
	error (1, errno, _("cannot seek on output"));
      /* fix juo -- is this copy_tape_buf_out?  or copy_disk? */
      tape_buffered_write (tmp_buf, out_file_des, useful_bytes_in_block);
      free (tmp_buf);
    }

  /* We are done reading the archive, so clear these since they
     will now be used for reading in files that we are appending
     to the archive.  */
  input_size = 0;
  input_bytes = 0;
  in_buff = input_buffer;
}

/* Support for remembering inodes with multiple links.  Used in the
   "copy in" and "copy pass" modes for making links instead of copying
   the file.  */

struct inode_val
{
  unsigned long inode;
  unsigned long major_num;
  unsigned long minor_num;
  char *file_name;
};

/* Inode hash table.  Allocated by first call to add_inode.  */
static struct inode_val **hash_table = NULL;

/* Size of current hash table.  Initial size is 47.  (47 = 2*22 + 3) */
static int hash_size = 22;

/* Number of elements in current hash table.  */
static int hash_num;

/* Find the file name associated with NODE_NUM.  If there is no file
   associated with NODE_NUM, return NULL.  */

char *
find_inode_file (unsigned long node_num, unsigned long major_num,
		 unsigned long minor_num)
{
  int start;			/* Initial hash location.  */
  int temp;			/* Rehash search variable.  */

  if (hash_table != NULL)
    {
      /* Hash function is node number modulo the table size.  */
      start = node_num % hash_size;

      /* Initial look into the table.  */
      if (hash_table[start] == NULL)
	return NULL;
      if (hash_table[start]->inode == node_num
	  && hash_table[start]->major_num == major_num
	  && hash_table[start]->minor_num == minor_num)
	return hash_table[start]->file_name;

      /* The home position is full with a different inode record.
	 Do a linear search terminated by a NULL pointer.  */
      for (temp = (start + 1) % hash_size;
	   hash_table[temp] != NULL && temp != start;
	   temp = (temp + 1) % hash_size)
	{
	  if (hash_table[temp]->inode == node_num
	      && hash_table[start]->major_num == major_num
	      && hash_table[start]->minor_num == minor_num)
	    return hash_table[temp]->file_name;
	}
    }
  return NULL;
}

/* Do the hash insert.  Used in normal inserts and resizing the hash
   table.  It is guaranteed that there is room to insert the item.
   NEW_VALUE is the pointer to the previously allocated inode, file
   name association record.  */

static void
hash_insert (struct inode_val *new_value)
{
  int start;			/* Home position for the value.  */
  int temp;			/* Used for rehashing.  */

  /* Hash function is node number modulo the table size.  */
  start = new_value->inode % hash_size;

  /* Do the initial look into the table.  */
  if (hash_table[start] == NULL)
    {
      hash_table[start] = new_value;
      return;
    }

  /* If we get to here, the home position is full with a different inode
     record.  Do a linear search for the first NULL pointer and insert
     the new item there.  */
  temp = (start + 1) % hash_size;
  while (hash_table[temp] != NULL)
    temp = (temp + 1) % hash_size;

  /* Insert at the NULL.  */
  hash_table[temp] = new_value;
}

/* Associate FILE_NAME with the inode NODE_NUM.  (Insert into hash table.)  */

void
add_inode (unsigned long node_num, char *file_name, unsigned long major_num,
	   unsigned long minor_num)
{
  struct inode_val *temp;

  /* Create new inode record.  */
  temp = (struct inode_val *) xmalloc (sizeof (struct inode_val));
  temp->inode = node_num;
  temp->major_num = major_num;
  temp->minor_num = minor_num;
  temp->file_name = xstrdup (file_name);

  /* Do we have to increase the size of (or initially allocate)
     the hash table?  */
  if (hash_num == hash_size || hash_table == NULL)
    {
      struct inode_val **old_table;	/* Pointer to old table.  */
      int i;			/* Index for re-insert loop.  */

      /* Save old table.  */
      old_table = hash_table;
      if (old_table == NULL)
	hash_num = 0;

      /* Calculate new size of table and allocate it.
         Sequence of table sizes is 47, 97, 197, 397, 797, 1597, 3197, 6397 ...
	 where 3197 and most of the sizes after 6397 are not prime.  The other
	 numbers listed are prime.  */
      hash_size = 2 * hash_size + 3;
      hash_table = (struct inode_val **)
	xmalloc (hash_size * sizeof (struct inode_val *));
      bzero (hash_table, hash_size * sizeof (struct inode_val *));

      /* Insert the values from the old table into the new table.  */
      for (i = 0; i < hash_num; i++)
	hash_insert (old_table[i]);

      if (old_table != NULL)
	free (old_table);
    }

  /* Insert the new record and increment the count of elements in the
      hash table.  */
  hash_insert (temp);
  hash_num++;
}


/* Open FILE in the mode specified by the command line options
   and return an open file descriptor for it,
   or -1 if it can't be opened.  */

int
open_archive (char *file)
{
  int fd;
  void (*copy_in) ();		/* Workaround for pcc bug.  */

  copy_in = process_copy_in;

  if (copy_function == copy_in)
    fd = rmtopen (file, O_RDONLY | O_BINARY, 0666, rsh_command_option);
  else
    {
      if (!append_flag)
	fd = rmtopen (file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666,
			rsh_command_option);
      else
	fd = rmtopen (file, O_RDWR | O_BINARY, 0666, rsh_command_option);
    }

  return fd;
}

/* Attempt to rewind the tape drive on file descriptor TAPE_DES
   and take it offline.  */

void
tape_offline (int tape_des)
{
#if defined(MTIOCTOP) && defined(MTOFFL)
  struct mtop control;

  control.mt_op = MTOFFL;
  control.mt_count = 1;
  rmtioctl (tape_des, MTIOCTOP, (char*) &control);	/* Don't care if it fails.  */
#endif
}

/* The file on file descriptor TAPE_DES is assumed to be magnetic tape
   (or floppy disk or other device) and the end of the medium
   has been reached.  Ask the user for to mount a new "tape" to continue
   the processing.  If the user specified the device name on the
   command line (with the -I, -O, -F or --file options), then we can
   automatically re-open the same device to use the next medium.  If the
   user did not specify the device name, then we have to ask them which
   device to use.  */

void
get_next_reel (int tape_des)
{
  static int reel_number = 1;
  FILE *tty_in;			/* File for interacting with user.  */
  FILE *tty_out;		/* File for interacting with user.  */
  int old_tape_des;
  char *next_archive_name;
  dynamic_string new_name;
  char *str_res;

  ds_init (&new_name, 128);

  /* Open files for interactive communication.  */
  tty_in = fopen (TTY_NAME, "r");
  if (tty_in == NULL)
    error (2, errno, TTY_NAME);
  tty_out = fopen (TTY_NAME, "w");
  if (tty_out == NULL)
    error (2, errno, TTY_NAME);

  old_tape_des = tape_des;
  tape_offline (tape_des);
  rmtclose (tape_des);

  /* Give message and wait for carrage return.  User should hit carrage return
     only after loading the next tape.  */
  ++reel_number;
  if (new_media_message)
    fprintf (tty_out, "%s", new_media_message);
  else if (new_media_message_with_number)
    fprintf (tty_out, "%s%d%s", new_media_message_with_number, reel_number,
	     new_media_message_after_number);
  else if (archive_name)
    fprintf (tty_out, _("Found end of volume.  Load next volume and press RETURN. "));
  else
    fprintf (tty_out, _("Found end of volume.  To continue, type device/file name when ready.\n"));

  fflush (tty_out);

  if (archive_name)
    {
      int c;

      do
	c = getc (tty_in);
      while (c != EOF && c != '\n');

      tape_des = open_archive (archive_name);
      if (tape_des == -1)
	error (1, errno, "%s", archive_name);
    }
  else
    {
      do
	{
	  if (tape_des < 0)
	    {
	      fprintf (tty_out,
		       _("To continue, type device/file name when ready.\n"));
	      fflush (tty_out);
	    }

	  str_res = ds_fgets (tty_in, &new_name);
	  if (str_res == NULL || str_res[0] == '\0')
	    exit (1);
	  next_archive_name = str_res;

	  tape_des = open_archive (next_archive_name);
	  if (tape_des == -1)
	    error (0, errno, "%s", next_archive_name);
	}
      while (tape_des < 0);
    }

  /* We have to make sure that `tape_des' has not changed its value even
     though we closed it and reopened it, since there are local
     copies of it in other routines.  This works fine on Unix (even with
     rmtread and rmtwrite) since open will always return the lowest
     available file descriptor and we haven't closed any files (e.g.,
     stdin, stdout or stderr) that were opened before we originally opened
     the archive.  */

  if (tape_des != old_tape_des)
    error (1, 0, _("internal error: tape descriptor changed from %d to %d"),
	   old_tape_des, tape_des);

  free (new_name.ds_string);
  fclose (tty_in);
  fclose (tty_out);
}

/* If MESSAGE does not contain the string "%d", make `new_media_message'
   a copy of MESSAGE.  If MESSAGES does contain the string "%d", make
   `new_media_message_with_number' a copy of MESSAGE up to, but
   not including, the string "%d", and make `new_media_message_after_number'
   a copy of MESSAGE after the string "%d".  */

void
set_new_media_message (char *message)
{
  char *p;
  int prev_was_percent;

  p = message;
  prev_was_percent = 0;
  while (*p != '\0')
    {
      if (*p == 'd' && prev_was_percent)
	break;
      prev_was_percent = (*p == '%');
      ++p;
    }
  if (*p == '\0')
    {
      new_media_message = xstrdup (message);
    }
  else
    {
      int length = p - message - 1;

      new_media_message_with_number = xmalloc (length + 1);
      strncpy (new_media_message_with_number, message, length);
      new_media_message_with_number[length] = '\0';
      length = strlen (p + 1);
      new_media_message_after_number = xmalloc (length + 1);
      strcpy (new_media_message_after_number, p + 1);
    }
}

#ifdef SYMLINK_USES_UMASK
/* Most machines always create symlinks with rwxrwxrwx protection,
   but some (HP/UX 8.07; maybe DEC's OSF on MIPS, too?) use the
   umask when creating symlinks, so if your umask is 022 you end
   up with rwxr-xr-x symlinks (although HP/UX seems to completely
   ignore the protection).  There doesn't seem to be any way to
   manipulate the modes once the symlinks are created (e.g.
   a hypothetical "lchmod"), so to create them with the right
   modes we have to set the umask first.  */

int
umasked_symlink (char *name1, char *name2, int mode)
{
  int	old_umask;
  int	rc;
  mode = ~(mode & 0777) & 0777;
  old_umask = umask (mode);
  rc = symlink (name1, name2);
  umask (old_umask);
  return rc;
}
#endif /* SYMLINK_USES_UMASK */

#ifdef HPUX_CDF
/* When we create a cpio archive we mark CDF's by putting an extra `/'
   after their component name so we can distinguish the CDF's when we
   extract the archive (in case the "hidden" directory's files appear
   in the archive before the directory itself).  E.g., in the path
   "a/b+/c", if b+ is a CDF, we will write this path as "a/b+//c" in
   the archive so when we extract the archive we will know that b+
   is actually a CDF, and not an ordinary directory whose name happens
   to end in `+'.  We also do the same thing internally in copypass.c.  */


/* Take an input pathname and check it for CDF's.  Insert an extra
   `/' in the pathname after each "hidden" directory.  If we add
   any `/'s, return a malloced string (which it will reuse for
   later calls so our caller doesn't have to worry about freeing
   the string) instead of the original input string.  */

char *
add_cdf_double_slashes (char *input_name)
{
  static char *ret_name = NULL;	/* re-usuable return buffer (malloc'ed)  */
  static int ret_size = -1;	/* size of return buffer.  */
  char *p;
  char *q;
  int n;
  struct stat dir_stat;

  /*  Search for a `/' preceeded by a `+'.  */

  for (p = input_name; *p != '\0'; ++p)
    {
      if ( (*p == '+') && (*(p + 1) == '/') )
	break;
    }

  /* If we didn't find a `/' preceeded by a `+' then there are
     no CDF's in this pathname.  Return the original pathname.  */

  if (*p == '\0')
    return input_name;

  /* There was a `/' preceeded by a `+' in the pathname.  If it is a CDF 
     then we will need to copy the input pathname to our return
     buffer so we can insert the extra `/'s.  Since we can't tell
     yet whether or not it is a CDF we will just always copy the
     string to the return buffer.  First we have to make sure the
     buffer is large enough to hold the string and any number of
     extra `/'s we might add.  */

  n = 2 * (strlen (input_name) + 1);
  if (n >= ret_size)
    {
      if (ret_size < 0)
	ret_name = (char *) malloc (n);
      else
	ret_name = (char *)realloc (ret_name, n);
      ret_size = n;
    }

  /* Clear the `/' after this component, so we can stat the pathname 
     up to and including this component.  */
  ++p;
  *p = '\0';
  if ((*xstat) (input_name, &dir_stat) < 0)
    {
      error (0, errno, "%s", input_name);
      return input_name;
    }

  /* Now put back the `/' after this component and copy the pathname up to
     and including this component and its trailing `/' to the return
     buffer.  */
  *p++ = '/';
  strncpy (ret_name, input_name, p - input_name);
  q = ret_name + (p - input_name);

  /* If it was a CDF, add another `/'.  */
  if (S_ISDIR (dir_stat.st_mode) && (dir_stat.st_mode & 04000) )
    *q++ = '/';

  /* Go through the rest of the input pathname, copying it to the
     return buffer, and adding an extra `/' after each CDF.  */
  while (*p != '\0')
    {
      if ( (*p == '+') && (*(p + 1) == '/') )
	{
	  *q++ = *p++;

	  *p = '\0';
	  if ((*xstat) (input_name, &dir_stat) < 0)
	    {
	      error (0, errno, "%s", input_name);
	      return input_name;
	    }
	  *p = '/';

	  if (S_ISDIR (dir_stat.st_mode) && (dir_stat.st_mode & 04000) )
	    *q++ = '/';
	}
      *q++ = *p++;
    }
  *q = '\0';

  return ret_name;
}

/* Is the last parent directory (e.g., c in a/b/c/d) a CDF?  If the
   directory name ends in `+' and is followed by 2 `/'s instead of 1
   then it is.  This is only the case for cpio archives, but we don't
   have to worry about tar because tar always has the directory before
   its files (or else we lose).  */
int
islastparentcdf (char *path)
{
  char *newpath;
  char *slash;
  int slash_count;
  int length;			/* Length of result, not including NUL.  */

  slash = rindex (path, '/');
  if (slash == 0)
    return 0;
  else
    {
      slash_count = 0;
      while (slash > path && *slash == '/')
	{
	  ++slash_count;
	  --slash;
	}


      if ( (*slash == '+') && (slash_count >= 2) )
	return 1;
    }
  return 0;
}
#endif

#define DISKBLOCKSIZE	(512)

enum sparse_write_states { begin, in_zeros, not_in_zeros };


static int
buf_all_zeros (char *buf, int bufsize)
{
  int	i;
  for (i = 0; i < bufsize; ++i)
    {
      if (*buf++ != '\0')
	return 0;
    }
  return 1;
}

int delayed_seek_count = 0;

/* Write NBYTE bytes from BUF to remote tape connection FILDES.
   Return the number of bytes written on success, -1 on error.  */

int
sparse_write (int fildes, char *buf, unsigned int nbyte)
{
  int complete_block_count;
  int leftover_bytes_count;
  int seek_count;
  int write_count;
  char *cur_write_start;
  int lseek_rc;
  int write_rc;
  int i;
  enum sparse_write_states state;

  complete_block_count = nbyte / DISKBLOCKSIZE;
  leftover_bytes_count = nbyte % DISKBLOCKSIZE;

  if (delayed_seek_count != 0)
    state = in_zeros;
  else
    state = begin;

  seek_count = delayed_seek_count;

  for (i = 0; i < complete_block_count; ++i)
    {
      switch (state)
	{
	  case begin :
	    if (buf_all_zeros (buf, DISKBLOCKSIZE))
	      {
		seek_count = DISKBLOCKSIZE;
		state = in_zeros;
	      }
	    else
	      {
		cur_write_start = buf;
		write_count = DISKBLOCKSIZE;
		state = not_in_zeros;
	      }
	    buf += DISKBLOCKSIZE;
	    break;
	  case in_zeros :
	    if (buf_all_zeros (buf, DISKBLOCKSIZE))
	      {
		seek_count += DISKBLOCKSIZE;
	      }
	    else
	      {
		lseek (fildes, seek_count, SEEK_CUR);
		cur_write_start = buf;
		write_count = DISKBLOCKSIZE;
		state = not_in_zeros;
	      }
	    buf += DISKBLOCKSIZE;
	    break;
	  case not_in_zeros :
	    if (buf_all_zeros (buf, DISKBLOCKSIZE))
	      {
		write_rc = write (fildes, cur_write_start, write_count);
		seek_count = DISKBLOCKSIZE;
		state = in_zeros;
	      }
	    else
	      {
		write_count += DISKBLOCKSIZE;
	      }
	    buf += DISKBLOCKSIZE;
	    break;
	}
    }

  switch (state)
    {
      case begin :
      case in_zeros :
	delayed_seek_count = seek_count;
	break;
      case not_in_zeros :
	write_rc = write (fildes, cur_write_start, write_count);
	delayed_seek_count = 0;
	break;
    }

  if (leftover_bytes_count != 0)
    {
      if (delayed_seek_count != 0)
	{
	  lseek_rc = lseek (fildes, delayed_seek_count, SEEK_CUR);
	  delayed_seek_count = 0;
	}
      write_rc = write (fildes, buf, leftover_bytes_count);
    }
  return nbyte;
}
