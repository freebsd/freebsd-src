/* List a tar archive.
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
 * List a tar archive.
 *
 * Also includes support routines for reading a tar archive.
 *
 * this version written 26 Aug 1985 by John Gilmore (ihnp4!hoptoad!gnu).
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <errno.h>
#ifndef STDC_HEADERS
extern int errno;
#endif
#include <time.h>

#ifdef BSD42
#include <sys/file.h>
#else
#ifndef V7
#include <fcntl.h>
#endif
#endif

#define	isodigit(c)	( ((c) >= '0') && ((c) <= '7') )

#include "tar.h"
#include "port.h"

extern FILE *msg_file;

long from_oct ();		/* Decode octal number */
void demode ();			/* Print file mode */
void restore_saved_dir_info ();
PTR ck_malloc ();

union record *head;		/* Points to current archive header */
struct stat hstat;		/* Stat struct corresponding */
int head_standard;		/* Tape header is in ANSI format */

int check_exclude ();
void close_archive ();
void decode_header ();
int findgid ();
int finduid ();
void name_gather ();
int name_match ();
void names_notfound ();
void open_archive ();
void print_header ();
int read_header ();
void saverec ();
void skip_file ();
void skip_extended_headers ();

extern char *quote_copy_string ();


/*
 * Main loop for reading an archive.
 */
void
read_and (do_something)
     void (*do_something) ();
{
  int status = 3;		/* Initial status at start of archive */
  int prev_status;
  extern time_t new_time;
  char save_linkflag;

  name_gather ();		/* Gather all the names */
  open_archive (1);		/* Open for reading */

  for (;;)
    {
      prev_status = status;
      status = read_header ();
	/* check if the namelist got emptied during the course of reading */
	/* the tape, if so stop by setting status to EOF */
      if ((namelist == NULL) && nlpsfreed) {
	  status = EOF;
      }
      switch (status)
	{

	case 1:		/* Valid header */
	  /* We should decode next field (mode) first... */
	  /* Ensure incoming names are null terminated. */

	  if (!name_match (current_file_name)
	      || (f_new_files && hstat.st_mtime < new_time)
	      || (f_exclude && check_exclude (current_file_name)))
	    {

	      int isextended = 0;

	      if (head->header.linkflag == LF_VOLHDR
		  || head->header.linkflag == LF_MULTIVOL
		  || head->header.linkflag == LF_NAMES)
		{
		  (*do_something) ();
		  continue;
		}
	      if (f_show_omitted_dirs
		  && head->header.linkflag == LF_DIR)
		msg ("Omitting %s\n", current_file_name);
	      /* Skip past it in the archive */
	      if (head->header.isextended)
		isextended = 1;
	      save_linkflag = head->header.linkflag;
	      userec (head);
	      if (isextended)
		{
		  /*					register union record *exhdr;

					for (;;) {
					    exhdr = findrec();
					    if (!exhdr->ext_hdr.isextended) {
					    	userec(exhdr);
					    	break;
					    }
					}
					userec(exhdr);*/
		  skip_extended_headers ();
		}
	      /* Skip to the next header on the archive */
	      if (save_linkflag != LF_DIR)
		skip_file ((long) hstat.st_size);
	      continue;

	    }

	  (*do_something) ();
	  continue;

	  /*
			 * If the previous header was good, tell them
			 * that we are skipping bad ones.
			 */
	case 0:		/* Invalid header */
	  userec (head);
	  switch (prev_status)
	    {
	    case 3:		/* Error on first record */
	      msg ("Hmm, this doesn't look like a tar archive.");
	      /* FALL THRU */
	    case 2:		/* Error after record of zeroes */
	    case 1:		/* Error after header rec */
	      msg ("Skipping to next file header...");
	    case 0:		/* Error after error */
	      break;
	    }
	  continue;

	case 2:		/* Record of zeroes */
	  userec (head);
	  status = prev_status;	/* If error after 0's */
	  if (f_ignorez)
	    continue;
	  /* FALL THRU */
	case EOF:		/* End of archive */
	  break;
	}
      break;
    };

  restore_saved_dir_info ();
  close_archive ();
  names_notfound ();		/* Print names not found */
}


/*
 * Print a header record, based on tar options.
 */
void
list_archive ()
{
  extern char *save_name;
  int isextended = 0;		/* Flag to remember if head is extended */

  /* Save the record */
  saverec (&head);

  /* Print the header record */
  if (f_verbose)
    {
      if (f_verbose > 1)
	decode_header (head, &hstat, &head_standard, 0);
      print_header ();
    }

  if (f_gnudump && head->header.linkflag == LF_DUMPDIR)
    {
      size_t size, written, check;
      char *data;
      extern long save_totsize;
      extern long save_sizeleft;

      userec (head);
      if (f_multivol)
	{
	  save_name = current_file_name;
	  save_totsize = hstat.st_size;
	}
      for (size = hstat.st_size; size > 0; size -= written)
	{
	  if (f_multivol)
	    save_sizeleft = size;
	  data = findrec ()->charptr;
	  if (data == NULL)
	    {
	      msg ("EOF in archive file?");
	      break;
	    }
	  written = endofrecs ()->charptr - data;
	  if (written > size)
	    written = size;
	  errno = 0;
	  check = fwrite (data, sizeof (char), written, msg_file);
	  userec ((union record *) (data + written - 1));
	  if (check != written)
	    {
	      msg_perror ("only wrote %ld of %ld bytes to file %s", check, written, current_file_name);
	      skip_file ((long) (size) - written);
	      break;
	    }
	}
      if (f_multivol)
	save_name = 0;
      saverec ((union record **) 0);	/* Unsave it */
      fputc ('\n', msg_file);
      fflush (msg_file);
      return;

    }
  saverec ((union record **) 0);/* Unsave it */
  /* Check to see if we have an extended header to skip over also */
  if (head->header.isextended)
    isextended = 1;

  /* Skip past the header in the archive */
  userec (head);

  /*
 	 * If we needed to skip any extended headers, do so now, by
 	 * reading extended headers and skipping past them in the
	 * archive.
	 */
  if (isextended)
    {
      /*		register union record *exhdr;

		for (;;) {
			exhdr = findrec();

			if (!exhdr->ext_hdr.isextended) {
				userec(exhdr);
				break;
			}
			userec(exhdr);
		}*/
      skip_extended_headers ();
    }

  if (f_multivol)
    save_name = current_file_name;
  /* Skip to the next header on the archive */

  skip_file ((long) hstat.st_size);

  if (f_multivol)
    save_name = 0;
}


/*
 * Read a record that's supposed to be a header record.
 * Return its address in "head", and if it is good, the file's
 * size in hstat.st_size.
 *
 * Return 1 for success, 0 if the checksum is bad, EOF on eof,
 * 2 for a record full of zeros (EOF marker).
 *
 * You must always userec(head) to skip past the header which this
 * routine reads.
 */
int
read_header ()
{
  register int i;
  register long sum, signed_sum, recsum;
  register char *p;
  register union record *header;
  long from_oct ();
  char **longp;
  char *bp, *data;
  int size, written;
  static char *next_long_name, *next_long_link;
  char *name;

recurse:

  header = findrec ();
  head = header;		/* This is our current header */
  if (NULL == header)
    return EOF;

  recsum = from_oct (8, header->header.chksum);

  signed_sum = sum = 0;
  p = header->charptr;
  for (i = sizeof (*header); --i >= 0;)
    {
      /*
		 * We can't use unsigned char here because of old compilers,
		 * e.g. V7.
		 */
      signed_sum += *p;
      sum += 0xFF & *p++;
    }

  /* Adjust checksum to count the "chksum" field as blanks. */
  for (i = sizeof (header->header.chksum); --i >= 0;)
    {
      sum -= 0xFF & header->header.chksum[i];
      signed_sum -= (char) header->header.chksum[i];
    }
  sum += ' ' * sizeof header->header.chksum;
  signed_sum += ' ' * sizeof header->header.chksum;

  if (sum == 8 * ' ')
    {
      /*
		 * This is a zeroed record...whole record is 0's except
		 * for the 8 blanks we faked for the checksum field.
		 */
      return 2;
    }

  if (sum != recsum && signed_sum != recsum)
    return 0;

  /*
	 * Good record.  Decode file size and return.
	 */
  if (header->header.linkflag == LF_LINK)
    hstat.st_size = 0;		/* Links 0 size on tape */
  else
    hstat.st_size = from_oct (1 + 12, header->header.size);

  header->header.arch_name[NAMSIZ - 1] = '\0';
  if (header->header.linkflag == LF_LONGNAME
      || header->header.linkflag == LF_LONGLINK)
    {
      longp = ((header->header.linkflag == LF_LONGNAME)
	       ? &next_long_name
	       : &next_long_link);

      userec (header);
      if (*longp)
	free (*longp);
      bp = *longp = (char *) ck_malloc (hstat.st_size);

      for (size = hstat.st_size;
	   size > 0;
	   size -= written)
	{
	  data = findrec ()->charptr;
	  if (data == NULL)
	    {
	      msg ("Unexpected EOF on archive file");
	      break;
	    }
	  written = endofrecs ()->charptr - data;
	  if (written > size)
	    written = size;

	  bcopy (data, bp, written);
	  bp += written;
	  userec ((union record *) (data + written - 1));
	}
      goto recurse;
    }
  else
    {
      name = (next_long_name
	      ? next_long_name
	      : head->header.arch_name);
      if (current_file_name)
	free (current_file_name);
      current_file_name = ck_malloc (strlen (name) + 1);
      strcpy (current_file_name, name);

      name = (next_long_link
	      ? next_long_link
	      : head->header.arch_linkname);
      if (current_link_name)
	free (current_link_name);
      current_link_name = ck_malloc (strlen (name) + 1);
      strcpy (current_link_name, name);

      next_long_link = next_long_name = 0;
      return 1;
    }
}


/*
 * Decode things from a file header record into a "struct stat".
 * Also set "*stdp" to !=0 or ==0 depending whether header record is "Unix
 * Standard" tar format or regular old tar format.
 *
 * read_header() has already decoded the checksum and length, so we don't.
 *
 * If wantug != 0, we want the uid/group info decoded from Unix Standard
 * tapes (for extraction).  If == 0, we are just printing anyway, so save time.
 *
 * decode_header should NOT be called twice for the same record, since the
 * two calls might use different "wantug" values and thus might end up with
 * different uid/gid for the two calls.  If anybody wants the uid/gid they
 * should decode it first, and other callers should decode it without uid/gid
 * before calling a routine, e.g. print_header, that assumes decoded data.
 */
void
decode_header (header, st, stdp, wantug)
     register union record *header;
     register struct stat *st;
     int *stdp;
     int wantug;
{
  long from_oct ();

  st->st_mode = from_oct (8, header->header.mode);
  st->st_mode &= 07777;
  st->st_mtime = from_oct (1 + 12, header->header.mtime);
  if (f_gnudump)
    {
      st->st_atime = from_oct (1 + 12, header->header.atime);
      st->st_ctime = from_oct (1 + 12, header->header.ctime);
    }

  if (0 == strcmp (header->header.magic, TMAGIC))
    {
      /* Unix Standard tar archive */
      *stdp = 1;
      if (wantug)
	{
#ifdef NONAMES
	  st->st_uid = from_oct (8, header->header.uid);
	  st->st_gid = from_oct (8, header->header.gid);
#else
	  st->st_uid =
	    (*header->header.uname
	     ? finduid (header->header.uname)
	     : from_oct (8, header->header.uid));
	  st->st_gid =
	    (*header->header.gname
	     ? findgid (header->header.gname)
	     : from_oct (8, header->header.gid));
#endif
	}
#if defined(S_IFBLK) || defined(S_IFCHR)
      switch (header->header.linkflag)
	{
	case LF_BLK:
	case LF_CHR:
	  st->st_rdev = makedev (from_oct (8, header->header.devmajor),
				 from_oct (8, header->header.devminor));
	}
#endif
    }
  else
    {
      /* Old fashioned tar archive */
      *stdp = 0;
      st->st_uid = from_oct (8, header->header.uid);
      st->st_gid = from_oct (8, header->header.gid);
      st->st_rdev = 0;
    }
}


/*
 * Quick and dirty octal conversion.
 *
 * Result is -1 if the field is invalid (all blank, or nonoctal).
 */
long
from_oct (digs, where)
     register int digs;
     register char *where;
{
  register long value;

  while (isspace ((unsigned char) *where))
    {				/* Skip spaces */
      where++;
      if (--digs <= 0)
	return -1;		/* All blank field */
    }
  value = 0;
  while (digs > 0 && isodigit (*where))
    {				/* Scan til nonoctal */
      value = (value << 3) | (*where++ - '0');
      --digs;
    }

  if (digs > 0 && *where && !isspace ((unsigned char) *where))
    return -1;			/* Ended on non-space/nul */

  return value;
}


/*
 * Actually print it.
 *
 * Plain and fancy file header block logging.
 * Non-verbose just prints the name, e.g. for "tar t" or "tar x".
 * This should just contain file names, so it can be fed back into tar
 * with xargs or the "-T" option.  The verbose option can give a bunch
 * of info, one line per file.  I doubt anybody tries to parse its
 * format, or if they do, they shouldn't.  Unix tar is pretty random here
 * anyway.
 *
 * Note that print_header uses the globals <head>, <hstat>, and
 * <head_standard>, which must be set up in advance.  This is not very clean
 * and should be cleaned up.  FIXME.
 */
#define	UGSWIDTH	18	/* min width of User, group, size */
/* UGSWIDTH of 18 means that with user and group names <= 8 chars the columns
   never shift during the listing.  */
#define	DATEWIDTH	19	/* Last mod date */
static int ugswidth = UGSWIDTH;	/* Max width encountered so far */

void
print_header ()
{
  char modes[11];
  char timestamp[80];
  char uform[11], gform[11];	/* These hold formatted ints */
  char *user, *group;
  char size[24];		/* Holds a formatted long or maj, min */
  time_t longie;
  int pad;
  char *name;
  extern long baserec;

  if (f_sayblock)
    fprintf (msg_file, "rec %10ld: ", baserec + (ar_record - ar_block));
  /* annofile(msg_file, (char *)NULL); */

  if (f_verbose <= 1)
    {
      /* Just the fax, mam. */
      char *name;

      name = quote_copy_string (current_file_name);
      if (name == 0)
	name = current_file_name;
      fprintf (msg_file, "%s\n", name);
      if (name != current_file_name)
	free (name);
    }
  else
    {
      /* File type and modes */
      modes[0] = '?';
      switch (head->header.linkflag)
	{
	case LF_VOLHDR:
	  modes[0] = 'V';
	  break;

	case LF_MULTIVOL:
	  modes[0] = 'M';
	  break;

	case LF_NAMES:
	  modes[0] = 'N';
	  break;

	case LF_LONGNAME:
	case LF_LONGLINK:
	  msg ("Visible longname error\n");
	  break;

	case LF_SPARSE:
	case LF_NORMAL:
	case LF_OLDNORMAL:
	case LF_LINK:
	  modes[0] = '-';
	  if ('/' == current_file_name[strlen (current_file_name) - 1])
	    modes[0] = 'd';
	  break;
	case LF_DUMPDIR:
	  modes[0] = 'd';
	  break;
	case LF_DIR:
	  modes[0] = 'd';
	  break;
	case LF_SYMLINK:
	  modes[0] = 'l';
	  break;
	case LF_BLK:
	  modes[0] = 'b';
	  break;
	case LF_CHR:
	  modes[0] = 'c';
	  break;
	case LF_FIFO:
	  modes[0] = 'p';
	  break;
	case LF_CONTIG:
	  modes[0] = 'C';
	  break;
	}

      demode ((unsigned) hstat.st_mode, modes + 1);

      /* Timestamp */
      longie = hstat.st_mtime;
      strftime(timestamp, sizeof(timestamp), "%c", localtime(&longie));
      timestamp[16] = '\0';
      timestamp[24] = '\0';

      /* User and group names */
      if (*head->header.uname && head_standard)
	{
	  user = head->header.uname;
	}
      else
	{
	  user = uform;
	  (void) sprintf (uform, "%ld",
			  from_oct (8, head->header.uid));
	}
      if (*head->header.gname && head_standard)
	{
	  group = head->header.gname;
	}
      else
	{
	  group = gform;
	  (void) sprintf (gform, "%ld",
			  from_oct (8, head->header.gid));
	}

      /* Format the file size or major/minor device numbers */
      switch (head->header.linkflag)
	{
#if defined(S_IFBLK) || defined(S_IFCHR)
	case LF_CHR:
	case LF_BLK:
	  (void) sprintf (size, "%d,%d",
			  major (hstat.st_rdev),
			  minor (hstat.st_rdev));
	  break;
#endif
	case LF_SPARSE:
	  (void) sprintf (size, "%ld",
			  from_oct (1 + 12, head->header.realsize));
	  break;
	default:
	  (void) sprintf (size, "%ld", (long) hstat.st_size);
	}

      /* Figure out padding and print the whole line. */
      pad = strlen (user) + strlen (group) + strlen (size) + 1;
      if (pad > ugswidth)
	ugswidth = pad;

      name = quote_copy_string (current_file_name);
      if (!name)
	name = current_file_name;
      fprintf (msg_file, "%s %s/%s %*s%s %s %s %s",
	       modes,
	       user,
	       group,
	       ugswidth - pad,
	       "",
	       size,
	       timestamp + 4, timestamp + 20,
	       name);

      if (name != current_file_name)
	free (name);
      switch (head->header.linkflag)
	{
	case LF_SYMLINK:
	  name = quote_copy_string (current_link_name);
	  if (!name)
	    name = current_link_name;
	  fprintf (msg_file, " -> %s\n", name);
	  if (name != current_link_name)
	    free (name);
	  break;

	case LF_LINK:
	  name = quote_copy_string (current_link_name);
	  if (!name)
	    name = current_link_name;
	  fprintf (msg_file, " link to %s\n", current_link_name);
	  if (name != current_link_name)
	    free (name);
	  break;

	default:
	  fprintf (msg_file, " unknown file type '%c'\n",
		   head->header.linkflag);
	  break;

	case LF_OLDNORMAL:
	case LF_NORMAL:
	case LF_SPARSE:
	case LF_CHR:
	case LF_BLK:
	case LF_DIR:
	case LF_FIFO:
	case LF_CONTIG:
	case LF_DUMPDIR:
	  putc ('\n', msg_file);
	  break;

	case LF_VOLHDR:
	  fprintf (msg_file, "--Volume Header--\n");
	  break;

	case LF_MULTIVOL:
	  fprintf (msg_file, "--Continued at byte %ld--\n", from_oct (1 + 12, head->header.offset));
	  break;

	case LF_NAMES:
	  fprintf (msg_file, "--Mangled file names--\n");
	  break;
	}
    }
  fflush (msg_file);
}

/*
 * Print a similar line when we make a directory automatically.
 */
void
pr_mkdir (pathname, length, mode)
     char *pathname;
     int length;
     int mode;
{
  char modes[11];
  char *name;
  extern long baserec;

  if (f_verbose > 1)
    {
      /* File type and modes */
      modes[0] = 'd';
      demode ((unsigned) mode, modes + 1);

      if (f_sayblock)
	fprintf (msg_file, "rec %10ld: ", baserec + (ar_record - ar_block));
      /* annofile(msg_file, (char *)NULL); */
      name = quote_copy_string (pathname);
      if (!name)
	name = pathname;
      fprintf (msg_file, "%s %*s %.*s\n",
	       modes,
	       ugswidth + DATEWIDTH,
	       "Creating directory:",
	       length,
	       pathname);
      if (name != pathname)
	free (name);
    }
}


/*
 * Skip over <size> bytes of data in records in the archive.
 */
void
skip_file (size)
     register long size;
{
  union record *x;
  extern long save_totsize;
  extern long save_sizeleft;

  if (f_multivol)
    {
      save_totsize = size;
      save_sizeleft = size;
    }

  while (size > 0)
    {
      x = findrec ();
      if (x == NULL)
	{			/* Check it... */
	  msg ("Unexpected EOF on archive file");
	  exit (EX_BADARCH);
	}
      userec (x);
      size -= RECORDSIZE;
      if (f_multivol)
	save_sizeleft -= RECORDSIZE;
    }
}

void
skip_extended_headers ()
{
  register union record *exhdr;

  for (;;)
    {
      exhdr = findrec ();
      if (!exhdr->ext_hdr.isextended)
	{
	  userec (exhdr);
	  break;
	}
      userec (exhdr);
    }
}

/*
 * Decode the mode string from a stat entry into a 9-char string and a null.
 */
void
demode (mode, string)
     register unsigned mode;
     register char *string;
{
  register unsigned mask;
  register char *rwx = "rwxrwxrwx";

  for (mask = 0400; mask != 0; mask >>= 1)
    {
      if (mode & mask)
	*string++ = *rwx++;
      else
	{
	  *string++ = '-';
	  rwx++;
	}
    }

  if (mode & S_ISUID)
    if (string[-7] == 'x')
      string[-7] = 's';
    else
      string[-7] = 'S';
  if (mode & S_ISGID)
    if (string[-4] == 'x')
      string[-4] = 's';
    else
      string[-4] = 'S';
  if (mode & S_ISVTX)
    if (string[-1] == 'x')
      string[-1] = 't';
    else
      string[-1] = 'T';
  *string = '\0';
}
