/* listing.c - mainting assembly listings
   Copyright (C) 1991, 1992 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

/*
 Contributed by Steve Chamberlain
 		sac@cygnus.com


 A listing page looks like:

 LISTING_HEADER  sourcefilename pagenumber
 TITLE LINE
 SUBTITLE LINE
 linenumber address data  source
 linenumber address data  source
 linenumber address data  source
 linenumber address data  source

 If not overridden, the listing commands are:

 .title  "stuff"
 	Put "stuff" onto the title line
 .sbttl  "stuff"
        Put stuff onto the subtitle line

  If these commands come within 10 lines of the top of the page, they
  will affect the page they are on, as well as any subsequent page

 .eject
 	Thow a page
 .list
 	Increment the enable listing counter
 .nolist
 	Decrement the enable listing counter

 .psize Y[,X]
 	Set the paper size to X wide and Y high. Setting a psize Y of
	zero will suppress form feeds except where demanded by .eject

 If the counter goes below zero, listing is suppressed.


 Listings are a maintained by read calling various listing_<foo>
 functions.  What happens most is that the macro NO_LISTING is not
 defined (from the Makefile), then the macro LISTING_NEWLINE expands
 into a call to listing_newline.  The call is done from read.c, every
 time it sees a newline, and -l is on the command line.

 The function listing_newline remembers the frag associated with the
 newline, and creates a new frag - note that this is wasteful, but not
 a big deal, since listing slows things down a lot anyway.  The
 function also rememebers when the filename changes.

 When all the input has finished, and gas has had a chance to settle
 down, the listing is output. This is done by running down the list of
 frag/source file records, and opening the files as needed and printing
 out the bytes and chars associated with them.

 The only things which the architecture can change about the listing
 are defined in these macros:

 LISTING_HEADER		The name of the architecture
 LISTING_WORD_SIZE      The make of the number of bytes in a word, this determines
 			the clumping of the output data. eg a value of
			2 makes words look like 1234 5678, whilst 1
			would make the same value look like 12 34 56
			78
 LISTING_LHS_WIDTH      Number of words of above size for the lhs

 LISTING_LHS_WIDTH_SECOND   Number of words for the data on the lhs
 			for the second line

 LISTING_LHS_CONT_LINES	Max number of lines to use up for a continutation
 LISTING_RHS_WIDTH      Number of chars from the input file to print
                        on a line
*/

#ifndef lint
static char rcsid[] = "$FreeBSD: src/gnu/usr.bin/as/listing.c,v 1.6 1999/08/27 23:34:18 peter Exp $";
#endif

#include <ctype.h>

#include "as.h"
#include <obstack.h>
#include "input-file.h"
#include "subsegs.h"

#ifndef NO_LISTING
#ifndef LISTING_HEADER
#define LISTING_HEADER "GAS LISTING"
#endif
#ifndef LISTING_WORD_SIZE
#define LISTING_WORD_SIZE 4
#endif
#ifndef LISTING_LHS_WIDTH
#define LISTING_LHS_WIDTH 1
#endif
#ifndef LISTING_LHS_WIDTH_SECOND
#define LISTING_LHS_WIDTH_SECOND 1
#endif
#ifndef LISTING_RHS_WIDTH
#define LISTING_RHS_WIDTH 100
#endif
#ifndef LISTING_LHS_CONT_LINES
#define LISTING_LHS_CONT_LINES 4
#endif




static struct list_info_struct *head;
struct list_info_struct *listing_tail;
extern int listing;
extern fragS *frag_now;


static int paper_width = 200;
static int paper_height = 60;


/* this static array is used to keep the text of data to be printed
   before the start of the line.
    It is stored so we can give a bit more info on the next line.  To much, and large
   initialized arrays will use up lots of paper.
 */

static char data_buffer[100];
static unsigned int data_buffer_size;


/* Prototypes.  */
static void listing_message PARAMS ((const char *name, const char *message));
static file_info_type *file_info PARAMS ((const char *file_name));
static void new_frag PARAMS ((void));
static char *buffer_line PARAMS ((file_info_type *file,
				  char *line, unsigned int size));
static void listing_page PARAMS ((list_info_type *list));
static unsigned int calc_hex PARAMS ((list_info_type *list));
static void print_lines PARAMS ((list_info_type *list,
				 char *string,
				 unsigned int address));
static void list_symbol_table PARAMS ((void));
static void print_source PARAMS ((file_info_type *current_file,
				  list_info_type *list,
				  char *buffer,
				  unsigned int width));
static int debugging_pseudo PARAMS ((char *line));
static void listing_listing PARAMS ((char *name));


static void
listing_message (name, message)
     const char *name;
     const char *message;
{
  unsigned int l = strlen (name) + strlen (message) + 1;
  char *n = (char *) xmalloc (l);
  strcpy (n, name);
  strcat (n, message);
  if (listing_tail != (list_info_type *) NULL)
    {
      listing_tail->message = n;
    }
}

void
listing_warning (message)
     const char *message;
{
  listing_message ("Warning:", message);
}

void
listing_error (message)
     const char *message;
{
  listing_message ("Error:", message);
}




static file_info_type *file_info_head;

static file_info_type *
file_info (file_name)
     const char *file_name;
{
  /* Find an entry with this file name */
  file_info_type *p = file_info_head;

  while (p != (file_info_type *) NULL)
    {
      if (strcmp (p->filename, file_name) == 0)
	return p;
      p = p->next;
    }

  /* Make new entry */

  p = (file_info_type *) xmalloc (sizeof (file_info_type));
  p->next = file_info_head;
  file_info_head = p;
  p->filename = xmalloc ((unsigned long) strlen (file_name) + 1);
  strcpy (p->filename, file_name);
  p->linenum = 0;
  p->end_pending = 0;

  /* Do we really prefer binary mode for this??  */
#define FOPEN_RB "r"
  p->file = fopen (p->filename, FOPEN_RB);
  if (p->file)
    fgetc (p->file);

  return p;
}


static void
new_frag ()
{

  frag_wane (frag_now);
  frag_new (0);

}

void
listing_newline (ps)
     char *ps;
{
  char *file;
  unsigned int line;
  static unsigned int last_line = 0xffff;
  static char *last_file = NULL;
  list_info_type *new;

  as_where (&file, &line);
  if (line != last_line || last_file && file && strcmp(file, last_file))
    {
      last_line = line;
      last_file = file;
      new_frag ();

      new = (list_info_type *) xmalloc (sizeof (list_info_type));
      new->frag = frag_now;
      new->line = line;
      new->file = file_info (file);

      if (listing_tail)
	{
	  listing_tail->next = new;
	}
      else
	{
	  head = new;
	}
      listing_tail = new;
      new->next = (list_info_type *) NULL;
      new->message = (char *) NULL;
      new->edict = EDICT_NONE;
      new->hll_file = (file_info_type *) NULL;
      new->hll_line = 0;
      new_frag ();
    }
}

/* Attach all current frags to the previous line instead of the
   current line.  This is called by the MIPS backend when it discovers
   that it needs to add some NOP instructions; the added NOP
   instructions should go with the instruction that has the delay, not
   with the new instruction.  */

void
listing_prev_line ()
{
  list_info_type *l;
  fragS *f;

  if (head == (list_info_type *) NULL
      || head == listing_tail)
    return;

  new_frag ();

  for (l = head; l->next != listing_tail; l = l->next)
    ;

  for (f = frchain_now->frch_root; f != (fragS *) NULL; f = f->fr_next)
    if (f->line == listing_tail)
      f->line = l;

  listing_tail->frag = frag_now;
  new_frag ();
}

/*
 This function returns the next source line from the file supplied,
 truncated to size.  It appends a fake line to the end of each input
 file to make
*/

static char *
buffer_line (file, line, size)
     file_info_type * file;
     char *line;
     unsigned int size;
{
  unsigned int count = 0;
  int c;

  char *p = line;

  /* If we couldn't open the file, return an empty line */
  if (file->file == (FILE *) NULL)
    {
      return "";
    }

  if (file->linenum == 0)
    rewind (file->file);

  if (file->end_pending == 10)
    {
      *p++ = '\n';
      fseek (file->file, 0, 0);
      file->linenum = 0;
      file->end_pending = 0;
    }
  c = fgetc (file->file);


  size -= 1;			/* leave room for null */

  while (c != EOF && c != '\n')
    {
      if (count < size)
	*p++ = c;
      count++;

      c = fgetc (file->file);

    }
  if (c == EOF)
    {
      file->end_pending++;
      *p++ = '.';
      *p++ = '.';
      *p++ = '.';
    }
  file->linenum++;
  *p++ = 0;
  return line;
}


static const char *fn;

static unsigned int eject;	/* Eject pending */
static unsigned int page;	/* Current page number */
static char *title;		/* current title */
static char *subtitle;		/* current subtitle */
static unsigned int on_page;	/* number of lines printed on current page */


static void
listing_page (list)
     list_info_type *list;
{
  /* Grope around, see if we can see a title or subtitle edict coming up
     soon  (we look down 10 lines of the page and see if it's there)*/
  if ((eject || (on_page >= paper_height)) && paper_height != 0)
    {
      unsigned int c = 10;
      int had_title = 0;
      int had_subtitle = 0;

      page++;

      while (c != 0 && list)
	{
	  if (list->edict == EDICT_SBTTL && !had_subtitle)
	    {
	      had_subtitle = 1;
	      subtitle = list->edict_arg;
	    }
	  if (list->edict == EDICT_TITLE && !had_title)
	    {
	      had_title = 1;
	      title = list->edict_arg;
	    }
	  list = list->next;
	  c--;
	}


      if (page > 1)
	{
	  printf ("\f");
	}

      printf ("%s %s \t\t\tpage %d\n", LISTING_HEADER, fn, page);
      printf ("%s\n", title);
      printf ("%s\n", subtitle);
      on_page = 3;
      eject = 0;
    }
}


static unsigned int
calc_hex (list)
     list_info_type * list;
{
  list_info_type *first = list;
  unsigned int address = (unsigned int) ~0;

  fragS *frag;
  fragS *frag_ptr;

  unsigned int byte_in_frag;


  /* Find first frag which says it belongs to this line */
  frag = list->frag;
  while (frag && frag->line != list)
    frag = frag->fr_next;

  frag_ptr = frag;

  data_buffer_size = 0;

  /* Dump all the frags which belong to this line */
  while (frag_ptr != (fragS *) NULL && frag_ptr->line == first)
    {
      /* Print as many bytes from the fixed part as is sensible */
      byte_in_frag = 0;
      while (byte_in_frag < frag_ptr->fr_fix && data_buffer_size < sizeof (data_buffer) - 10)
	{
	  if (address == ~0)
	    {
	      address = frag_ptr->fr_address;
	    }

	  sprintf (data_buffer + data_buffer_size,
		   "%02X",
		   (frag_ptr->fr_literal[byte_in_frag]) & 0xff);
	  data_buffer_size += 2;
	  byte_in_frag++;
	}
      {
	unsigned int var_rep_max = byte_in_frag;
	unsigned int var_rep_idx = byte_in_frag;

	/* Print as many bytes from the variable part as is sensible */
	while (byte_in_frag < frag_ptr->fr_var * frag_ptr->fr_offset
	       && data_buffer_size < sizeof (data_buffer) - 10)
	  {
	    if (address == ~0)
	      {
		address = frag_ptr->fr_address;
	      }
	    sprintf (data_buffer + data_buffer_size,
		     "%02X",
		     (frag_ptr->fr_literal[var_rep_idx]) & 0xff);
#if 0
	    data_buffer[data_buffer_size++] = '*';
	    data_buffer[data_buffer_size++] = '*';
#endif
	    data_buffer_size += 2;

	    var_rep_idx++;
	    byte_in_frag++;

	    if (var_rep_idx >= frag_ptr->fr_var)
	      var_rep_idx = var_rep_max;
	  }
      }

      frag_ptr = frag_ptr->fr_next;
    }
  data_buffer[data_buffer_size++] = 0;
  return address;
}






static void
print_lines (list, string, address)
     list_info_type *list;
     char *string;
     unsigned int address;
{
  unsigned int idx;
  unsigned int nchars;
  unsigned int lines;
  unsigned int byte_in_word = 0;
  char *src = data_buffer;

  /* Print the stuff on the first line */
  listing_page (list);
  nchars = (LISTING_WORD_SIZE * 2 + 1) * LISTING_LHS_WIDTH;
  /* Print the hex for the first line */
  if (address == ~0)
    {
      printf ("% 4d     ", list->line);
      for (idx = 0; idx < nchars; idx++)
	printf (" ");

      printf ("\t%s\n", string ? string : "");
      on_page++;
      listing_page (0);

    }
  else
    {
      if (had_errors ())
	{
	  printf ("% 4d ???? ", list->line);
	}
      else
	{
	  printf ("% 4d %04x ", list->line, address);
	}

      /* And the data to go along with it */
      idx = 0;

      while (*src && idx < nchars)
	{
	  printf ("%c%c", src[0], src[1]);
	  src += 2;
	  byte_in_word++;
	  if (byte_in_word == LISTING_WORD_SIZE)
	    {
	      printf (" ");
	      idx++;
	      byte_in_word = 0;
	    }
	  idx += 2;
	}

      for (; idx < nchars; idx++)
	printf (" ");

      printf ("\t%s\n", string ? string : "");
      on_page++;
      listing_page (list);
      if (list->message)
	{
	  printf ("****  %s\n", list->message);
	  listing_page (list);
	  on_page++;
	}

      for (lines = 0;
	   lines < LISTING_LHS_CONT_LINES
	   && *src;
	   lines++)
	{
	  nchars = ((LISTING_WORD_SIZE * 2) + 1) * LISTING_LHS_WIDTH_SECOND - 1;
	  idx = 0;
	  /* Print any more lines of data, but more compactly */
	  printf ("% 4d      ", list->line);

	  while (*src && idx < nchars)
	    {
	      printf ("%c%c", src[0], src[1]);
	      src += 2;
	      idx += 2;
	      byte_in_word++;
	      if (byte_in_word == LISTING_WORD_SIZE)
		{
		  printf (" ");
		  idx++;
		  byte_in_word = 0;
		}
	    }

	  printf ("\n");
	  on_page++;
	  listing_page (list);

	}


    }
}


static void
list_symbol_table ()
{
  extern symbolS *symbol_rootP;
  int got_some = 0;

  symbolS *ptr;
  eject = 1;
  listing_page (0);

  for (ptr = symbol_rootP; ptr != (symbolS *) NULL; ptr = symbol_next (ptr))
    {
      if (ptr->sy_frag->line)
	{
	  if (S_GET_NAME (ptr))
	    {
	      char buf[30];
	      valueT val = S_GET_VALUE (ptr);

	      /* @@ Note that this is dependent on the compilation options,
		 not solely on the target characteristics.  */
	      if (sizeof (val) == 4 && sizeof (int) == 4)
		sprintf (buf, "%08lx", (unsigned long) val);
#if defined (BFD64)
	      else if (sizeof (val) > 4)
		{
		  char buf1[30];
		  sprintf_vma (buf1, val);
		  strcpy (buf, "00000000");
		  strcpy (buf + 8 - strlen (buf1), buf1);
		}
#endif
	      else
		abort ();

	      if (!got_some)
		{
		  printf ("DEFINED SYMBOLS\n");
		  on_page++;
		  got_some = 1;
		}

	      printf ("%20s:%-5d  %s:%s %s\n",
		      ptr->sy_frag->line->file->filename,
		      ptr->sy_frag->line->line,
		      segment_name (S_GET_SEGMENT (ptr)),
		      buf, S_GET_NAME (ptr));

	      on_page++;
	      listing_page (0);
	    }
	}

    }
  if (!got_some)
    {
      printf ("NO DEFINED SYMBOLS\n");
      on_page++;
    }
  printf ("\n");
  on_page++;
  listing_page (0);

  got_some = 0;

  for (ptr = symbol_rootP; ptr != (symbolS *) NULL; ptr = symbol_next (ptr))
    {
      if (S_GET_NAME (ptr) && strlen (S_GET_NAME (ptr)) != 0)
	{
	  if (ptr->sy_frag->line == 0
#ifdef notyet
	      && S_GET_SEGMENT (ptr) != reg_section)
#else
	      && !S_IS_REGISTER(ptr))
#endif
	    {
	      if (!got_some)
		{
		  got_some = 1;
		  printf ("UNDEFINED SYMBOLS\n");
		  on_page++;
		  listing_page (0);
		}
	      printf ("%s\n", S_GET_NAME (ptr));
	      on_page++;
	      listing_page (0);
	    }
	}
    }
  if (!got_some)
    {
      printf ("NO UNDEFINED SYMBOLS\n");
      on_page++;
      listing_page (0);
    }
}

static void
print_source (current_file, list, buffer, width)
     file_info_type *current_file;
     list_info_type *list;
     char *buffer;
     unsigned int width;
{
  if (current_file->file)
    {
      while (current_file->linenum < list->hll_line)
	{
	  char *p = buffer_line (current_file, buffer, width);
	  printf ("%4d:%-13s **** %s\n", current_file->linenum, current_file->filename, p);
	  on_page++;
	  listing_page (list);
	}
    }
}

/* Sometimes the user doesn't want to be bothered by the debugging
   records inserted by the compiler, see if the line is suspicious */

static int
debugging_pseudo (line)
     char *line;
{
  while (isspace (*line))
    line++;

  if (*line != '.')
    return 0;

  line++;

  if (strncmp (line, "def", 3) == 0)
    return 1;
  if (strncmp (line, "val", 3) == 0)
    return 1;
  if (strncmp (line, "scl", 3) == 0)
    return 1;
  if (strncmp (line, "line", 4) == 0)
    return 1;
  if (strncmp (line, "endef", 5) == 0)
    return 1;
  if (strncmp (line, "ln", 2) == 0)
    return 1;
  if (strncmp (line, "type", 4) == 0)
    return 1;
  if (strncmp (line, "size", 4) == 0)
    return 1;
  if (strncmp (line, "dim", 3) == 0)
    return 1;
  if (strncmp (line, "tag", 3) == 0)
    return 1;

  if (strncmp (line, "stabs", 5) == 0)
    return 1;
  if (strncmp (line, "stabn", 5) == 0)
    return 1;

  return 0;

}

static void
listing_listing (name)
     char *name;
{
  list_info_type *list = head;
  file_info_type *current_hll_file = (file_info_type *) NULL;
  char *message;
  char *buffer;
  char *p;
  int show_listing = 1;
  unsigned int width;

  buffer = xmalloc (LISTING_RHS_WIDTH);
  eject = 1;
  list = head;

  while (list != (list_info_type *) NULL && 0)
    {
      if (list->next)
	list->frag = list->next->frag;
      list = list->next;

    }

  list = head->next;


  while (list)
    {
      width = LISTING_RHS_WIDTH > paper_width ? paper_width :
	LISTING_RHS_WIDTH;

      switch (list->edict)
	{
	case EDICT_LIST:
	  show_listing++;
	  break;
	case EDICT_NOLIST:
	  show_listing--;
	  break;
	case EDICT_EJECT:
	  break;
	case EDICT_NONE:
	  break;
	case EDICT_TITLE:
	  title = list->edict_arg;
	  break;
	case EDICT_SBTTL:
	  subtitle = list->edict_arg;
	  break;
	default:
	  abort ();
	}

      if (show_listing > 0)
	{
	  /* Scan down the list and print all the stuff which can be done
	     with this line (or lines).  */
	  message = 0;

	  if (list->hll_file)
	    {
	      current_hll_file = list->hll_file;
	    }

	  if (current_hll_file && list->hll_line && listing & LISTING_HLL)
	    {
	      print_source (current_hll_file, list, buffer, width);
	    }

	  while (list->file->file &&
		 list->file->linenum < list->line && !list->file->end_pending) {
		  p = buffer_line (list->file, buffer, width);

		  if (!((listing & LISTING_NODEBUG) && debugging_pseudo (p)))
		    {
		      print_lines (list, p, calc_hex (list));
		    }
	  }

	  if (list->edict == EDICT_EJECT)
	    {
	      eject = 1;
	    }
	}
      else
	{

	  while (list->file->file &&
		 list->file->linenum < list->line && !list->file->end_pending)
		  p = buffer_line (list->file, buffer, width);
	}

      list = list->next;
    }
  free (buffer);
}

void
listing_print (name)
     char *name;
{
  title = "";
  subtitle = "";

  if (listing & LISTING_NOFORM)
    {
      paper_height = 0;
    }

  if (listing & LISTING_LISTING)
    {
      listing_listing (name);

    }
  if (listing & LISTING_SYMBOLS)
    {
      list_symbol_table ();
    }
}


void
listing_file (name)
     const char *name;
{
  fn = name;
}

void
listing_eject (ignore)
     int ignore;
{
  listing_tail->edict = EDICT_EJECT;
}

void
listing_flags (ignore)
     int ignore;
{
  while ((*input_line_pointer++) && (*input_line_pointer != '\n'))
    input_line_pointer++;

}

void
listing_list (on)
     int on;
{
  listing_tail->edict = on ? EDICT_LIST : EDICT_NOLIST;
}


void
listing_psize (ignore)
     int ignore;
{
  paper_height = get_absolute_expression ();

  if (paper_height < 0 || paper_height > 1000)
    {
      paper_height = 0;
      as_warn ("strange paper height, set to no form");
    }
  if (*input_line_pointer == ',')
    {
      input_line_pointer++;
      paper_width = get_absolute_expression ();
    }
}


void
listing_title (depth)
     int depth;
{
  char *start;
  char *ttl;
  unsigned int length;

  SKIP_WHITESPACE ();
  if (*input_line_pointer == '\"')
    {
      input_line_pointer++;
      start = input_line_pointer;

      while (*input_line_pointer)
	{
	  if (*input_line_pointer == '\"')
	    {
	      length = input_line_pointer - start;
	      ttl = xmalloc (length + 1);
	      memcpy (ttl, start, length);
	      ttl[length] = 0;
	      listing_tail->edict = depth ? EDICT_SBTTL : EDICT_TITLE;
	      listing_tail->edict_arg = ttl;
	      input_line_pointer++;
	      demand_empty_rest_of_line ();
	      return;
	    }
	  else if (*input_line_pointer == '\n')
	    {
	      as_bad ("New line in title");
	      demand_empty_rest_of_line ();
	      return;
	    }
	  else
	    {
	      input_line_pointer++;
	    }
	}
    }
  else
    {
      as_bad ("expecting title in quotes");
    }
}



void
listing_source_line (line)
     unsigned int line;
{
  new_frag ();
  listing_tail->hll_line = line;
  new_frag ();

}

void
listing_source_file (file)
     const char *file;
{
  if (listing_tail)
    listing_tail->hll_file = file_info (file);
}



#else


/* Dummy functions for when compiled without listing enabled */

void
listing_flags (ignore)
     int ignore;
{
  s_ignore (0);
}

void
listing_list (on)
     int on;
{
  s_ignore (0);
}

void
listing_eject (ignore)
     int ignore;
{
  s_ignore (0);
}

void
listing_psize (ignore)
     int ignore;
{
  s_ignore (0);
}

void
listing_title (depth)
     int depth;
{
  s_ignore (0);
}

void
listing_file (name)
     const char *name;
{

}

void
listing_newline (name)
     char *name;
{

}

void
listing_source_line (n)
     unsigned int n;
{

}
void
listing_source_file (n)
     const char *n;
{

}

#endif
