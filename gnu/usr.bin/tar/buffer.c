/* Buffer management for tar.
   Copyright (C) 1988, 1992, 1993 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Buffer management for tar.
 *
 * Written by John Gilmore, ihnp4!hoptoad!gnu, on 25 August 1985.
 */

#include <stdio.h>
#include <errno.h>
#ifndef STDC_HEADERS
extern int errno;
#endif
#include <sys/types.h>		/* For non-Berkeley systems */
#include <signal.h>
#include <time.h>
time_t time ();

#ifdef HAVE_SYS_MTIO_H
#include <sys/ioctl.h>
#include <sys/mtio.h>
#endif

#ifdef BSD42
#include <sys/file.h>
#else
#ifndef V7
#include <fcntl.h>
#endif
#endif

#ifdef	__MSDOS__
#include <process.h>
#endif

#ifdef XENIX
#include <sys/inode.h>
#endif

#include "tar.h"
#include "port.h"
#include "rmt.h"
#include "regex.h"

/* Either stdout or stderr:  The thing we write messages (standard msgs, not
   errors) to.  Stdout unless we're writing a pipe, in which case stderr */
FILE *msg_file = stdout;

#define	STDIN	0		/* Standard input  file descriptor */
#define	STDOUT	1		/* Standard output file descriptor */

#define	PREAD	0		/* Read  file descriptor from pipe() */
#define	PWRITE	1		/* Write file descriptor from pipe() */

#define	MAGIC_STAT	105	/* Magic status returned by child, if
				   it can't exec.  We hope compress/sh
				   never return this status! */

void *valloc ();

void writeerror ();
void readerror ();

void ck_pipe ();
void ck_close ();

int backspace_output ();
extern void finish_header ();
void flush_archive ();
int isfile ();
int new_volume ();
void verify_volume ();
extern void to_oct ();

#ifndef __MSDOS__
/* Obnoxious test to see if dimwit is trying to dump the archive */
dev_t ar_dev;
ino_t ar_ino;
#endif

/*
 * The record pointed to by save_rec should not be overlaid
 * when reading in a new tape block.  Copy it to record_save_area first, and
 * change the pointer in *save_rec to point to record_save_area.
 * Saved_recno records the record number at the time of the save.
 * This is used by annofile() to print the record number of a file's
 * header record.
 */
static union record **save_rec;
union record record_save_area;
static long saved_recno;

/*
 * PID of child program, if f_compress or remote archive access.
 */
static int childpid = 0;

/*
 * Record number of the start of this block of records
 */
long baserec;

/*
 * Error recovery stuff
 */
static int r_error_count;

/*
 * Have we hit EOF yet?
 */
static int hit_eof;

/* Checkpointing counter */
static int checkpoint;

/* JF we're reading, but we just read the last record and its time to update */
extern time_to_start_writing;
int file_to_switch_to = -1;	/* If remote update, close archive, and use
				   this descriptor to write to */

static int volno = 1;		/* JF which volume of a multi-volume tape
				   we're on */
static int global_volno = 1;	/* Volume number to print in external messages. */

char *save_name = 0;		/* Name of the file we are currently writing */
long save_totsize;		/* total size of file we are writing.  Only
				   valid if save_name is non_zero */
long save_sizeleft;		/* Where we are in the file we are writing.
				   Only valid if save_name is non-zero */

int write_archive_to_stdout;

/* Used by fl_read and fl_write to store the real info about saved names */
static char real_s_name[NAMSIZ];
static long real_s_totsize;
static long real_s_sizeleft;

/* Reset the EOF flag (if set), and re-set ar_record, etc */

void
reset_eof ()
{
  if (hit_eof)
    {
      hit_eof = 0;
      ar_record = ar_block;
      ar_last = ar_block + blocking;
      ar_reading = 0;
    }
}

/*
 * Return the location of the next available input or output record.
 * Return NULL for EOF.  Once we have returned NULL, we just keep returning
 * it, to avoid accidentally going on to the next file on the "tape".
 */
union record *
findrec ()
{
  if (ar_record == ar_last)
    {
      if (hit_eof)
	return (union record *) NULL;	/* EOF */
      flush_archive ();
      if (ar_record == ar_last)
	{
	  hit_eof++;
	  return (union record *) NULL;	/* EOF */
	}
    }
  return ar_record;
}


/*
 * Indicate that we have used all records up thru the argument.
 * (should the arg have an off-by-1? XXX FIXME)
 */
void
userec (rec)
     union record *rec;
{
  while (rec >= ar_record)
    ar_record++;
  /*
	 * Do NOT flush the archive here.  If we do, the same
	 * argument to userec() could mean the next record (if the
	 * input block is exactly one record long), which is not what
	 * is intended.
	 */
  if (ar_record > ar_last)
    abort ();
}


/*
 * Return a pointer to the end of the current records buffer.
 * All the space between findrec() and endofrecs() is available
 * for filling with data, or taking data from.
 */
union record *
endofrecs ()
{
  return ar_last;
}


/*
 * Duplicate a file descriptor into a certain slot.
 * Equivalent to BSD "dup2" with error reporting.
 */
void
dupto (from, to, msg)
     int from, to;
     char *msg;
{
  int err;

  if (from != to)
    {
      err = close (to);
      if (err < 0 && errno != EBADF)
	{
	  msg_perror ("Cannot close descriptor %d", to);
	  exit (EX_SYSTEM);
	}
      err = dup (from);
      if (err != to)
	{
	  msg_perror ("cannot dup %s", msg);
	  exit (EX_SYSTEM);
	}
      ck_close (from);
    }
}

#ifdef __MSDOS__
void
child_open ()
{
  fprintf (stderr, "MS-DOS %s can't use compressed or remote archives\n", tar);
  exit (EX_ARGSBAD);
}

#else
void
child_open ()
{
  int pipe[2];
  int err = 0;

  int kidpipe[2];
  int kidchildpid;

#define READ	0
#define WRITE	1

  ck_pipe (pipe);

  childpid = fork ();
  if (childpid < 0)
    {
      msg_perror ("cannot fork");
      exit (EX_SYSTEM);
    }
  if (childpid > 0)
    {
      /* We're the parent.  Clean up and be happy */
      /* This, at least, is easy */

      if (ar_reading)
	{
	  f_reblock++;
	  archive = pipe[READ];
	  ck_close (pipe[WRITE]);
	}
      else
	{
	  archive = pipe[WRITE];
	  ck_close (pipe[READ]);
	}
      return;
    }

  /* We're the kid */
  if (ar_reading)
    {
      dupto (pipe[WRITE], STDOUT, "(child) pipe to stdout");
      ck_close (pipe[READ]);
    }
  else
    {
      dupto (pipe[READ], STDIN, "(child) pipe to stdin");
      ck_close (pipe[WRITE]);
    }

  /* We need a child tar only if
	   1: we're reading/writing stdin/out (to force reblocking)
	   2: the file is to be accessed by rmt (compress doesn't know how)
	   3: the file is not a plain file */
#ifdef NO_REMOTE
  if (!(ar_files[0][0] == '-' && ar_files[0][1] == '\0') && isfile (ar_files[0]))
#else
  if (!(ar_files[0][0] == '-' && ar_files[0][1] == '\0') && !_remdev (ar_files[0]) && isfile (ar_files[0]))
#endif
    {
      /* We don't need a child tar.  Open the archive */
      if (ar_reading)
	{
	  archive = open (ar_files[0], O_RDONLY | O_BINARY, 0666);
	  if (archive < 0)
	    {
	      msg_perror ("can't open archive %s", ar_files[0]);
	      exit (EX_BADARCH);
	    }
	  dupto (archive, STDIN, "archive to stdin");
	  /* close(archive); */
	}
      else
	{
	  archive = creat (ar_files[0], 0666);
	  if (archive < 0)
	    {
	      msg_perror ("can't open archive %s", ar_files[0]);
	      exit (EX_BADARCH);
	    }
	  dupto (archive, STDOUT, "archive to stdout");
	  /* close(archive); */
	}
    }
  else
    {
      /* We need a child tar */
      ck_pipe (kidpipe);

      kidchildpid = fork ();
      if (kidchildpid < 0)
	{
	  msg_perror ("child can't fork");
	  exit (EX_SYSTEM);
	}

      if (kidchildpid > 0)
	{
	  /* About to exec compress:  set up the files */
	  if (ar_reading)
	    {
	      dupto (kidpipe[READ], STDIN, "((child)) pipe to stdin");
	      ck_close (kidpipe[WRITE]);
	      /* dup2(pipe[WRITE],STDOUT); */
	    }
	  else
	    {
	      /* dup2(pipe[READ],STDIN); */
	      dupto (kidpipe[WRITE], STDOUT, "((child)) pipe to stdout");
	      ck_close (kidpipe[READ]);
	    }
	  /* ck_close(pipe[READ]); */
	  /* ck_close(pipe[WRITE]); */
	  /* ck_close(kidpipe[READ]);
			ck_close(kidpipe[WRITE]); */
	}
      else
	{
	  /* Grandchild.  Do the right thing, namely sit here and
		   read/write the archive, and feed stuff back to compress */
	  tar = "tar (child)";
	  if (ar_reading)
	    {
	      dupto (kidpipe[WRITE], STDOUT, "[child] pipe to stdout");
	      ck_close (kidpipe[READ]);
	    }
	  else
	    {
	      dupto (kidpipe[READ], STDIN, "[child] pipe to stdin");
	      ck_close (kidpipe[WRITE]);
	    }

	  if (ar_files[0][0] == '-' && ar_files[0][1] == '\0')
	    {
	      if (ar_reading)
		archive = STDIN;
	      else
		archive = STDOUT;
	    }
	  else			/* This can't happen if (ar_reading==2)
				archive = rmtopen(ar_files[0], O_RDWR|O_CREAT|O_BINARY, 0666);
	  	  			else */ if (ar_reading)
	    archive = rmtopen (ar_files[0], O_RDONLY | O_BINARY, 0666);
	  else
	    archive = rmtcreat (ar_files[0], 0666);

	  if (archive < 0)
	    {
	      msg_perror ("can't open archive %s", ar_files[0]);
	      exit (EX_BADARCH);
	    }

	  if (ar_reading)
	    {
	      for (;;)
		{
		  char *ptr;
		  int max, count;

		  r_error_count = 0;
		error_loop:
		  err = rmtread (archive, ar_block->charptr, (int) (blocksize));
		  if (err < 0)
		    {
		      readerror ();
		      goto error_loop;
		    }
		  if (err == 0)
		    break;
		  ptr = ar_block->charptr;
		  max = err;
		  while (max)
		    {
		      count = (max < RECORDSIZE) ? max : RECORDSIZE;
		      err = write (STDOUT, ptr, count);
		      if (err != count)
			{
			  if (err < 0)
			    {
			      msg_perror ("can't write to compression program");
			      exit (EX_SYSTEM);
			    }
			  else
			    msg ("write to compression program short %d bytes",
				 count - err);
			  count = (err < 0) ? 0 : err;
			}
		      ptr += count;
		      max -= count;
		    }
		}
	    }
	  else
	    {
	      for (;;)
		{
		  int n;
		  char *ptr;

		  n = blocksize;
		  ptr = ar_block->charptr;
		  while (n)
		    {
		      err = read (STDIN, ptr, (n < RECORDSIZE) ? n : RECORDSIZE);
		      if (err <= 0)
			break;
		      n -= err;
		      ptr += err;
		    }
		  /* EOF */
		  if (err == 0)
		    {
		      if (!f_compress_block)
			blocksize -= n;
		      else
			bzero (ar_block->charptr + blocksize - n, n);
		      err = rmtwrite (archive, ar_block->charptr, blocksize);
		      if (err != (blocksize))
			writeerror (err);
		      if (!f_compress_block)
			blocksize += n;
		      break;
		    }
		  if (n)
		    {
		      msg_perror ("can't read from compression program");
		      exit (EX_SYSTEM);
		    }
		  err = rmtwrite (archive, ar_block->charptr, (int) blocksize);
		  if (err != blocksize)
		    writeerror (err);
		}
	    }

	  /* close_archive(); */
	  exit (0);
	}
    }
  /* So we should exec compress (-d) */
  if (ar_reading)
    execlp (f_compressprog, f_compressprog, "-d", (char *) 0);
  else
    execlp (f_compressprog, f_compressprog, (char *) 0);
  msg_perror ("can't exec %s", f_compressprog);
  _exit (EX_SYSTEM);
}


/* return non-zero if p is the name of a directory */
int
isfile (p)
     char *p;
{
  struct stat stbuf;

  if (stat (p, &stbuf) < 0)
    return 1;
  if (S_ISREG (stbuf.st_mode))
    return 1;
  return 0;
}

#endif

/*
 * Open an archive file.  The argument specifies whether we are
 * reading or writing.
 */
/* JF if the arg is 2, open for reading and writing. */
void
open_archive (reading)
     int reading;
{
  msg_file = f_exstdout ? stderr : stdout;

  if (blocksize == 0)
    {
      msg ("invalid value for blocksize");
      exit (EX_ARGSBAD);
    }

  if (n_ar_files == 0)
    {
      msg ("No archive name given, what should I do?");
      exit (EX_BADARCH);
    }

  /*NOSTRICT*/
  if (f_multivol)
    {
      ar_block = (union record *) valloc ((unsigned) (blocksize + (2 * RECORDSIZE)));
      if (ar_block)
	ar_block += 2;
    }
  else
    ar_block = (union record *) valloc ((unsigned) blocksize);
  if (!ar_block)
    {
      msg ("could not allocate memory for blocking factor %d",
	   blocking);
      exit (EX_ARGSBAD);
    }

  ar_record = ar_block;
  ar_last = ar_block + blocking;
  ar_reading = reading;

  if (f_multivol && f_verify)
    {
      msg ("cannot verify multi-volume archives");
      exit (EX_ARGSBAD);
    }

  if (f_compressprog)
    {
      if (reading == 2 || f_verify)
	{
	  msg ("cannot update or verify compressed archives");
	  exit (EX_ARGSBAD);
	}
      if (f_multivol)
	{
	  msg ("cannot use multi-volume compressed archives");
	  exit (EX_ARGSBAD);
	}
      child_open ();
      if (!reading && ar_files[0][0] == '-' && ar_files[0][1] == '\0')
	msg_file = stderr;
      /* child_open(rem_host, rem_file); */
    }
  else if (ar_files[0][0] == '-' && ar_files[0][1] == '\0')
    {
      f_reblock++;		/* Could be a pipe, be safe */
      if (f_verify)
	{
	  msg ("can't verify stdin/stdout archive");
	  exit (EX_ARGSBAD);
	}
      if (reading == 2)
	{
	  archive = STDIN;
	  msg_file = stderr;
	  write_archive_to_stdout++;
	}
      else if (reading)
	archive = STDIN;
      else
	{
	  archive = STDOUT;
	  msg_file = stderr;
	}
    }
  else if (reading == 2 || f_verify)
    {
      archive = rmtopen (ar_files[0], O_RDWR | O_CREAT | O_BINARY, 0666);
    }
  else if (reading)
    {
      archive = rmtopen (ar_files[0], O_RDONLY | O_BINARY, 0666);
    }
  else
    {
      archive = rmtcreat (ar_files[0], 0666);
    }
  if (archive < 0)
    {
      msg_perror ("can't open %s", ar_files[0]);
      exit (EX_BADARCH);
    }
#ifndef __MSDOS__
  if (!_isrmt (archive))
    {
      struct stat tmp_stat;

      fstat (archive, &tmp_stat);
      if (S_ISREG (tmp_stat.st_mode))
	{
	  ar_dev = tmp_stat.st_dev;
	  ar_ino = tmp_stat.st_ino;
	}
    }
#endif

#ifdef	__MSDOS__
  setmode (archive, O_BINARY);
#endif

  if (reading)
    {
      ar_last = ar_block;	/* Set up for 1st block = # 0 */
      (void) findrec ();	/* Read it in, check for EOF */

      if (f_volhdr)
	{
	  union record *head;
#if 0
	  char *ptr;

	  if (f_multivol)
	    {
	      ptr = malloc (strlen (f_volhdr) + 20);
	      sprintf (ptr, "%s Volume %d", f_volhdr, 1);
	    }
	  else
	    ptr = f_volhdr;
#endif
	  head = findrec ();
	  if (!head)
	    {
	      msg ("Archive not labelled to match %s", f_volhdr);
	      exit (EX_BADVOL);
	    }
	  if (re_match (label_pattern, head->header.arch_name,
			strlen (head->header.arch_name), 0, 0) < 0)
	    {
	      msg ("Volume mismatch!  %s!=%s", f_volhdr,
		   head->header.arch_name);
	      exit (EX_BADVOL);
	    }
#if 0
	  if (strcmp (ptr, head->header.name))
	    {
	      msg ("Volume mismatch!  %s!=%s", ptr, head->header.name);
	      exit (EX_BADVOL);
	    }
	  if (ptr != f_volhdr)
	    free (ptr);
#endif
	}
    }
  else if (f_volhdr)
    {
      bzero ((void *) ar_block, RECORDSIZE);
      if (f_multivol)
	sprintf (ar_block->header.arch_name, "%s Volume 1", f_volhdr);
      else
	strcpy (ar_block->header.arch_name, f_volhdr);
      current_file_name = ar_block->header.arch_name;
      ar_block->header.linkflag = LF_VOLHDR;
      to_oct (time (0), 1 + 12, ar_block->header.mtime);
      finish_header (ar_block);
      /* ar_record++; */
    }
}


/*
 * Remember a union record * as pointing to something that we
 * need to keep when reading onward in the file.  Only one such
 * thing can be remembered at once, and it only works when reading
 * an archive.
 *
 * We calculate "offset" then add it because some compilers end up
 * adding (baserec+ar_record), doing a 9-bit shift of baserec, then
 * subtracting ar_block from that, shifting it back, losing the top 9 bits.
 */
void
saverec (pointer)
     union record **pointer;
{
  long offset;

  save_rec = pointer;
  offset = ar_record - ar_block;
  saved_recno = baserec + offset;
}

/*
 * Perform a write to flush the buffer.
 */

/*send_buffer_to_file();
  if(new_volume) {
  	deal_with_new_volume_stuff();
	send_buffer_to_file();
  }
 */

void
fl_write ()
{
  int err;
  int copy_back;
  static long bytes_written = 0;

  if (f_checkpoint && !(++checkpoint % 10))
    msg ("Write checkpoint %d\n", checkpoint);
  if (tape_length && bytes_written >= tape_length * 1024)
    {
      errno = ENOSPC;
      err = 0;
    }
  else
    err = rmtwrite (archive, ar_block->charptr, (int) blocksize);
  if (err != blocksize && !f_multivol)
    writeerror (err);
  else if (f_totals)
    tot_written += blocksize;

  if (err > 0)
    bytes_written += err;
  if (err == blocksize)
    {
      if (f_multivol)
	{
	  if (!save_name)
	    {
	      real_s_name[0] = '\0';
	      real_s_totsize = 0;
	      real_s_sizeleft = 0;
	      return;
	    }
#ifdef __MSDOS__
	  if (save_name[1] == ':')
	    save_name += 2;
#endif
	  while (*save_name == '/')
	    save_name++;

	  strcpy (real_s_name, save_name);
	  real_s_totsize = save_totsize;
	  real_s_sizeleft = save_sizeleft;
	}
      return;
    }

  /* We're multivol  Panic if we didn't get the right kind of response */
  /* ENXIO is for the UNIX PC */
  if (err < 0 && errno != ENOSPC && errno != EIO && errno != ENXIO)
    writeerror (err);

  /* If error indicates a short write, we just move to the next tape. */

  if (new_volume (0) < 0)
    return;
  bytes_written = 0;
  if (f_volhdr && real_s_name[0])
    {
      copy_back = 2;
      ar_block -= 2;
    }
  else if (f_volhdr || real_s_name[0])
    {
      copy_back = 1;
      ar_block--;
    }
  else
    copy_back = 0;
  if (f_volhdr)
    {
      bzero ((void *) ar_block, RECORDSIZE);
      sprintf (ar_block->header.arch_name, "%s Volume %d", f_volhdr, volno);
      to_oct (time (0), 1 + 12, ar_block->header.mtime);
      ar_block->header.linkflag = LF_VOLHDR;
      finish_header (ar_block);
    }
  if (real_s_name[0])
    {
      int tmp;

      if (f_volhdr)
	ar_block++;
      bzero ((void *) ar_block, RECORDSIZE);
      strcpy (ar_block->header.arch_name, real_s_name);
      ar_block->header.linkflag = LF_MULTIVOL;
      to_oct ((long) real_s_sizeleft, 1 + 12,
	      ar_block->header.size);
      to_oct ((long) real_s_totsize - real_s_sizeleft,
	      1 + 12, ar_block->header.offset);
      tmp = f_verbose;
      f_verbose = 0;
      finish_header (ar_block);
      f_verbose = tmp;
      if (f_volhdr)
	ar_block--;
    }

  err = rmtwrite (archive, ar_block->charptr, (int) blocksize);
  if (err != blocksize)
    writeerror (err);
  else if (f_totals)
    tot_written += blocksize;


  bytes_written = blocksize;
  if (copy_back)
    {
      ar_block += copy_back;
      bcopy ((void *) (ar_block + blocking - copy_back),
	     (void *) ar_record,
	     copy_back * RECORDSIZE);
      ar_record += copy_back;

      if (real_s_sizeleft >= copy_back * RECORDSIZE)
	real_s_sizeleft -= copy_back * RECORDSIZE;
      else if ((real_s_sizeleft + RECORDSIZE - 1) / RECORDSIZE <= copy_back)
	real_s_name[0] = '\0';
      else
	{
#ifdef __MSDOS__
	  if (save_name[1] == ':')
	    save_name += 2;
#endif
	  while (*save_name == '/')
	    save_name++;

	  strcpy (real_s_name, save_name);
	  real_s_sizeleft = save_sizeleft;
	  real_s_totsize = save_totsize;
	}
      copy_back = 0;
    }
}

/* Handle write errors on the archive.  Write errors are always fatal */
/* Hitting the end of a volume does not cause a write error unless the write
*  was the first block of the volume */

void
writeerror (err)
     int err;
{
  if (err < 0)
    {
      msg_perror ("can't write to %s", ar_files[cur_ar_file]);
      exit (EX_BADARCH);
    }
  else
    {
      msg ("only wrote %u of %u bytes to %s", err, blocksize, ar_files[cur_ar_file]);
      exit (EX_BADARCH);
    }
}

/*
 * Handle read errors on the archive.
 *
 * If the read should be retried, readerror() returns to the caller.
 */
void
readerror ()
{
#	define	READ_ERROR_MAX	10

  read_error_flag++;		/* Tell callers */

  msg_perror ("read error on %s", ar_files[cur_ar_file]);

  if (baserec == 0)
    {
      /* First block of tape.  Probably stupidity error */
      exit (EX_BADARCH);
    }

  /*
	 * Read error in mid archive.  We retry up to READ_ERROR_MAX times
	 * and then give up on reading the archive.  We set read_error_flag
	 * for our callers, so they can cope if they want.
	 */
  if (r_error_count++ > READ_ERROR_MAX)
    {
      msg ("Too many errors, quitting.");
      exit (EX_BADARCH);
    }
  return;
}


/*
 * Perform a read to flush the buffer.
 */
void
fl_read ()
{
  int err;			/* Result from system call */
  int left;			/* Bytes left */
  char *more;			/* Pointer to next byte to read */

  if (f_checkpoint && !(++checkpoint % 10))
    msg ("Read checkpoint %d\n", checkpoint);

  /*
	 * Clear the count of errors.  This only applies to a single
	 * call to fl_read.  We leave read_error_flag alone; it is
	 * only turned off by higher level software.
	 */
  r_error_count = 0;		/* Clear error count */

  /*
	 * If we are about to wipe out a record that
	 * somebody needs to keep, copy it out to a holding
	 * area and adjust somebody's pointer to it.
	 */
  if (save_rec &&
      *save_rec >= ar_record &&
      *save_rec < ar_last)
    {
      record_save_area = **save_rec;
      *save_rec = &record_save_area;
    }
  if (write_archive_to_stdout && baserec != 0)
    {
      err = rmtwrite (1, ar_block->charptr, blocksize);
      if (err != blocksize)
	writeerror (err);
    }
  if (f_multivol)
    {
      if (save_name)
	{
	  if (save_name != real_s_name)
	    {
#ifdef __MSDOS__
	      if (save_name[1] == ':')
		save_name += 2;
#endif
	      while (*save_name == '/')
		save_name++;

	      strcpy (real_s_name, save_name);
	      save_name = real_s_name;
	    }
	  real_s_totsize = save_totsize;
	  real_s_sizeleft = save_sizeleft;

	}
      else
	{
	  real_s_name[0] = '\0';
	  real_s_totsize = 0;
	  real_s_sizeleft = 0;
	}
    }

error_loop:
  err = rmtread (archive, ar_block->charptr, (int) blocksize);
  if (err == blocksize)
    return;

  if ((err == 0 || (err < 0 && errno == ENOSPC) || (err > 0 && !f_reblock)) && f_multivol)
    {
      union record *head;

    try_volume:
      if (new_volume ((cmd_mode == CMD_APPEND || cmd_mode == CMD_CAT || cmd_mode == CMD_UPDATE) ? 2 : 1) < 0)
	return;
    vol_error:
      err = rmtread (archive, ar_block->charptr, (int) blocksize);
      if (err < 0)
	{
	  readerror ();
	  goto vol_error;
	}
      if (err != blocksize)
	goto short_read;

      head = ar_block;

      if (head->header.linkflag == LF_VOLHDR)
	{
	  if (f_volhdr)
	    {
#if 0
	      char *ptr;

	      ptr = (char *) malloc (strlen (f_volhdr) + 20);
	      sprintf (ptr, "%s Volume %d", f_volhdr, volno);
#endif
	      if (re_match (label_pattern, head->header.arch_name,
			    strlen (head->header.arch_name),
			    0, 0) < 0)
		{
		  msg ("Volume mismatch! %s!=%s", f_volhdr,
		       head->header.arch_name);
		  --volno;
		  --global_volno;
		  goto try_volume;
		}

#if 0
	      if (strcmp (ptr, head->header.name))
		{
		  msg ("Volume mismatch! %s!=%s", ptr, head->header.name);
		  --volno;
		  --global_volno;
		  free (ptr);
		  goto try_volume;
		}
	      free (ptr);
#endif
	    }
	  if (f_verbose)
	    fprintf (msg_file, "Reading %s\n", head->header.arch_name);
	  head++;
	}
      else if (f_volhdr)
	{
	  msg ("Warning:  No volume header!");
	}

      if (real_s_name[0])
	{
	  long from_oct ();

	  if (head->header.linkflag != LF_MULTIVOL || strcmp (head->header.arch_name, real_s_name))
	    {
	      msg ("%s is not continued on this volume!", real_s_name);
	      --volno;
	      --global_volno;
	      goto try_volume;
	    }
	  if (real_s_totsize != from_oct (1 + 12, head->header.size) + from_oct (1 + 12, head->header.offset))
	    {
	      msg ("%s is the wrong size (%ld!=%ld+%ld)",
		   head->header.arch_name, save_totsize,
		   from_oct (1 + 12, head->header.size),
		   from_oct (1 + 12, head->header.offset));
	      --volno;
	      --global_volno;
	      goto try_volume;
	    }
	  if (real_s_totsize - real_s_sizeleft != from_oct (1 + 12, head->header.offset))
	    {
	      msg ("This volume is out of sequence");
	      --volno;
	      --global_volno;
	      goto try_volume;
	    }
	  head++;
	}
      ar_record = head;
      return;
    }
  else if (err < 0)
    {
      readerror ();
      goto error_loop;		/* Try again */
    }

short_read:
  more = ar_block->charptr + err;
  left = blocksize - err;

again:
  if (0 == (((unsigned) left) % RECORDSIZE))
    {
      /* FIXME, for size=0, multi vol support */
      /* On the first block, warn about the problem */
      if (!f_reblock && baserec == 0 && f_verbose && err > 0)
	{
	  /*	msg("Blocksize = %d record%s",
				err / RECORDSIZE, (err > RECORDSIZE)? "s": "");*/
	  msg ("Blocksize = %d records", err / RECORDSIZE);
	}
      ar_last = ar_block + ((unsigned) (blocksize - left)) / RECORDSIZE;
      return;
    }
  if (f_reblock)
    {
      /*
		 * User warned us about this.  Fix up.
		 */
      if (left > 0)
	{
	error2loop:
	  err = rmtread (archive, more, (int) left);
	  if (err < 0)
	    {
	      readerror ();
	      goto error2loop;	/* Try again */
	    }
	  if (err == 0)
	    {
	      msg ("archive %s EOF not on block boundary", ar_files[cur_ar_file]);
	      exit (EX_BADARCH);
	    }
	  left -= err;
	  more += err;
	  goto again;
	}
    }
  else
    {
      msg ("only read %d bytes from archive %s", err, ar_files[cur_ar_file]);
      exit (EX_BADARCH);
    }
}


/*
 * Flush the current buffer to/from the archive.
 */
void
flush_archive ()
{
  int c;

  baserec += ar_last - ar_block;/* Keep track of block #s */
  ar_record = ar_block;		/* Restore pointer to start */
  ar_last = ar_block + blocking;/* Restore pointer to end */

  if (ar_reading)
    {
      if (time_to_start_writing)
	{
	  time_to_start_writing = 0;
	  ar_reading = 0;

	  if (file_to_switch_to >= 0)
	    {
	      if ((c = rmtclose (archive)) < 0)
		msg_perror ("Warning: can't close %s(%d,%d)", ar_files[cur_ar_file], archive, c);

	      archive = file_to_switch_to;
	    }
	  else
	    (void) backspace_output ();
	  fl_write ();
	}
      else
	fl_read ();
    }
  else
    {
      fl_write ();
    }
}

/* Backspace the archive descriptor by one blocks worth.
   If its a tape, MTIOCTOP will work.  If its something else,
   we try to seek on it.  If we can't seek, we lose! */
int
backspace_output ()
{
  long cur;
  /* int er; */
  extern char *output_start;

#ifdef MTIOCTOP
  struct mtop t;

  t.mt_op = MTBSR;
  t.mt_count = 1;
  if ((rmtioctl (archive, MTIOCTOP, &t)) >= 0)
    return 1;
  if (errno == EIO && (rmtioctl (archive, MTIOCTOP, &t)) >= 0)
    return 1;
#endif

  cur = rmtlseek (archive, 0L, 1);
  cur -= blocksize;
  /* Seek back to the beginning of this block and
	   start writing there. */

  if (rmtlseek (archive, cur, 0) != cur)
    {
      /* Lseek failed.  Try a different method */
      msg ("Couldn't backspace archive file.  It may be unreadable without -i.");
      /* Replace the first part of the block with nulls */
      if (ar_block->charptr != output_start)
	bzero (ar_block->charptr, output_start - ar_block->charptr);
      return 2;
    }
  return 3;
}


/*
 * Close the archive file.
 */
void
close_archive ()
{
  int child;
  int status;
  int c;

  if (time_to_start_writing || !ar_reading)
    flush_archive ();
  if (cmd_mode == CMD_DELETE)
    {
      off_t pos;

      pos = rmtlseek (archive, 0L, 1);
#ifndef __MSDOS__
      (void) ftruncate (archive, pos);
#else
      (void) rmtwrite (archive, "", 0);
#endif
    }
  if (f_verify)
    verify_volume ();

  if ((c = rmtclose (archive)) < 0)
    msg_perror ("Warning: can't close %s(%d,%d)", ar_files[cur_ar_file], archive, c);

#ifndef	__MSDOS__
  if (childpid)
    {
      /*
       * Loop waiting for the right child to die, or for
       * no more kids.
       */
      while (((child = wait (&status)) != childpid) && child != -1)
	;

      if (child != -1)
	{
	  if (WIFSIGNALED (status))
	    {
	      /* SIGPIPE is OK, everything else is a problem. */
	      if (WTERMSIG (status) != SIGPIPE)
		msg ("child died with signal %d%s", WTERMSIG (status),
		     WIFCOREDUMPED (status) ? " (core dumped)" : "");
	    }
	  else
	    {
	      /* Child voluntarily terminated  -- but why? */
	      if (WEXITSTATUS (status) == MAGIC_STAT)
		{
		  exit (EX_SYSTEM);	/* Child had trouble */
		}
	      if (WEXITSTATUS (status) == (SIGPIPE + 128))
		{
		  /*
		   * /bin/sh returns this if its child
		   * dies with SIGPIPE.  'Sok.
		   */
		  /* Do nothing. */
		}
	      else if (WEXITSTATUS (status))
		msg ("child returned status %d",
		     WEXITSTATUS (status));
	    }
	}
    }
#endif /* __MSDOS__ */
}


#ifdef DONTDEF
/*
 * Message management.
 *
 * anno writes a message prefix on stream (eg stdout, stderr).
 *
 * The specified prefix is normally output followed by a colon and a space.
 * However, if other command line options are set, more output can come
 * out, such as the record # within the archive.
 *
 * If the specified prefix is NULL, no output is produced unless the
 * command line option(s) are set.
 *
 * If the third argument is 1, the "saved" record # is used; if 0, the
 * "current" record # is used.
 */
void
anno (stream, prefix, savedp)
     FILE *stream;
     char *prefix;
     int savedp;
{
#	define	MAXANNO	50
  char buffer[MAXANNO];		/* Holds annorecment */
#	define	ANNOWIDTH 13
  int space;
  long offset;
  int save_e;

  save_e = errno;
  /* Make sure previous output gets out in sequence */
  if (stream == stderr)
    fflush (stdout);
  if (f_sayblock)
    {
      if (prefix)
	{
	  fputs (prefix, stream);
	  putc (' ', stream);
	}
      offset = ar_record - ar_block;
      (void) sprintf (buffer, "rec %d: ",
		      savedp ? saved_recno :
		      baserec + offset);
      fputs (buffer, stream);
      space = ANNOWIDTH - strlen (buffer);
      if (space > 0)
	{
	  fprintf (stream, "%*s", space, "");
	}
    }
  else if (prefix)
    {
      fputs (prefix, stream);
      fputs (": ", stream);
    }
  errno = save_e;
}

#endif

/* Called to initialize the global volume number. */
void
init_volume_number ()
{
  FILE *vf;

  vf = fopen (f_volno_file, "r");
  if (!vf && errno != ENOENT)
    msg_perror ("%s", f_volno_file);

  if (vf)
    {
      fscanf (vf, "%d", &global_volno);
      fclose (vf);
    }
}

/* Called to write out the closing global volume number. */
void
closeout_volume_number ()
{
  FILE *vf;

  vf = fopen (f_volno_file, "w");
  if (!vf)
    msg_perror ("%s", f_volno_file);
  else
    {
      fprintf (vf, "%d\n", global_volno);
      fclose (vf);
    }
}

/* We've hit the end of the old volume.  Close it and open the next one */
/* Values for type:  0: writing  1: reading  2: updating */
int
new_volume (type)
     int type;
{
  int c;
  char inbuf[80];
  char *p;
  static FILE *read_file = 0;
  extern int now_verifying;
  extern char TTY_NAME[];
  static int looped = 0;

  if (!read_file && !f_run_script_at_end)
    read_file = (archive == 0) ? fopen (TTY_NAME, "r") : stdin;

  if (now_verifying)
    return -1;
  if (f_verify)
    verify_volume ();
  if ((c = rmtclose (archive)) < 0)
    msg_perror ("Warning: can't close %s(%d,%d)", ar_files[cur_ar_file], archive, c);

  global_volno++;
  volno++;
  cur_ar_file++;
  if (cur_ar_file == n_ar_files)
    {
      cur_ar_file = 0;
      looped = 1;
    }

tryagain:
  if (looped)
    {
      /* We have to prompt from now on. */
      if (f_run_script_at_end)
	{
	  closeout_volume_number ();
	  system (info_script);
	}
      else
	for (;;)
	  {
	    fprintf (msg_file, "\007Prepare volume #%d for %s and hit return: ", global_volno, ar_files[cur_ar_file]);
	    fflush (msg_file);
	    if (fgets (inbuf, sizeof (inbuf), read_file) == 0)
	      {
		fprintf (msg_file, "EOF?  What does that mean?");
		if (cmd_mode != CMD_EXTRACT && cmd_mode != CMD_LIST && cmd_mode != CMD_DIFF)
		  msg ("Warning:  Archive is INCOMPLETE!");
		exit (EX_BADARCH);
	      }
	    if (inbuf[0] == '\n' || inbuf[0] == 'y' || inbuf[0] == 'Y')
	      break;

	    switch (inbuf[0])
	      {
	      case '?':
		{
		  fprintf (msg_file, "\
 n [name]   Give a new filename for the next (and subsequent) volume(s)\n\
 q          Abort tar\n\
 !          Spawn a subshell\n\
 ?          Print this list\n");
		}
		break;

	      case 'q':	/* Quit */
		fprintf (msg_file, "No new volume; exiting.\n");
		if (cmd_mode != CMD_EXTRACT && cmd_mode != CMD_LIST && cmd_mode != CMD_DIFF)
		  msg ("Warning:  Archive is INCOMPLETE!");
		exit (EX_BADARCH);

	      case 'n':	/* Get new file name */
		{
		  char *q, *r;
		  static char *old_name;

		  for (q = &inbuf[1]; *q == ' ' || *q == '\t'; q++)
		    ;
		  for (r = q; *r; r++)
		    if (*r == '\n')
		      *r = '\0';
		  old_name = p = (char *) malloc ((unsigned) (strlen (q) + 2));
		  if (p == 0)
		    {
		      msg ("Can't allocate memory for name");
		      exit (EX_SYSTEM);
		    }
		  (void) strcpy (p, q);
		  ar_files[cur_ar_file] = p;
		}
		break;

	      case '!':
#ifdef __MSDOS__
		spawnl (P_WAIT, getenv ("COMSPEC"), "-", 0);
#else
		/* JF this needs work! */
		switch (fork ())
		  {
		  case -1:
		    msg_perror ("can't fork!");
		    break;
		  case 0:
		    p = getenv ("SHELL");
		    if (p == 0)
		      p = "/bin/sh";
		    execlp (p, "-sh", "-i", 0);
		    msg_perror ("can't exec a shell %s", p);
		    _exit (55);
		  default:
		    wait (0);
		    break;
		  }
#endif
		break;
	      }
	  }
    }


  if (type == 2 || f_verify)
    archive = rmtopen (ar_files[cur_ar_file], O_RDWR | O_CREAT, 0666);
  else if (type == 1)
    archive = rmtopen (ar_files[cur_ar_file], O_RDONLY, 0666);
  else if (type == 0)
    archive = rmtcreat (ar_files[cur_ar_file], 0666);
  else
    archive = -1;

  if (archive < 0)
    {
      msg_perror ("can't open %s", ar_files[cur_ar_file]);
      goto tryagain;
    }
#ifdef __MSDOS__
  setmode (archive, O_BINARY);
#endif
  return 0;
}

/* this is a useless function that takes a buffer returned by wantbytes
   and does nothing with it.  If the function called by wantbytes returns
   an error indicator (non-zero), this function is called for the rest of
   the file.
 */
int
no_op (size, data)
     int size;
     char *data;
{
  return 0;
}

/* Some other routine wants SIZE bytes in the archive.  For each chunk of
   the archive, call FUNC with the size of the chunk, and the address of
   the chunk it can work with.
 */
int
wantbytes (size, func)
     long size;
     int (*func) ();
{
  char *data;
  long data_size;

  while (size)
    {
      data = findrec ()->charptr;
      if (data == NULL)
	{			/* Check it... */
	  msg ("Unexpected EOF on archive file");
	  return -1;
	}
      data_size = endofrecs ()->charptr - data;
      if (data_size > size)
	data_size = size;
      if ((*func) (data_size, data))
	func = no_op;
      userec ((union record *) (data + data_size - 1));
      size -= data_size;
    }
  return 0;
}
