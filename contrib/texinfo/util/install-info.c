/* install-info -- create Info directory entry(ies) for an Info file.
   $Id: install-info.c,v 1.48 1999/08/06 18:13:32 karl Exp $
   $FreeBSD: src/contrib/texinfo/util/install-info.c,v 1.11 2000/01/24 16:05:17 ru Exp $

   Copyright (C) 1996, 97, 98, 99 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.*/

#include "system.h"
#include <getopt.h>

static char *progname = "install-info";
static char *default_section = NULL;

struct line_data *findlines ();
void insert_entry_here ();
int compare_section_names (), compare_entries_text ();

struct spec_entry;

/* Data structures.  */


/* Record info about a single line from a file as read into core.  */
struct line_data
{
  /* The start of the line.  */
  char *start;
  /* The number of characters in the line,
     excluding the terminating newline.  */
  int size;
  /* Vector containing pointers to the entries to add before this line.
     The vector is null-terminated.  */
  struct spec_entry **add_entries_before;
  /* 1 means output any needed new sections before this line.  */
  int add_sections_before;
  /* 1 means don't output this line.  */
  int delete;
};


/* This is used for a list of the specified menu section names
   in which entries should be added.  */
struct spec_section
{
  struct spec_section *next;
  char *name;
  /* 1 means we have not yet found an existing section with this name
     in the dir file--so we will need to add a new section.  */
  int missing;
};


/* This is used for a list of the entries specified to be added.  */
struct spec_entry
{
  struct spec_entry *next;
  char *text;
  int text_len;
  /* A pointer to the list of sections to which this entry should be
     added.  */
  struct spec_section *entry_sections;
  /* A pointer to a section that is beyond the end of the chain whose
     head is pointed to by entry_sections.  */
  struct spec_section *entry_sections_tail;
};


/* This is used for a list of nodes found by parsing the dir file.  */
struct node
{
  struct node *next;
  /* The node name.  */
  char *name;
  /* The line number of the line where the node starts.
     This is the line that contains control-underscore.  */
  int start_line;
  /* The line number of the line where the node ends,
     which is the end of the file or where the next line starts.  */
  int end_line;
  /* Start of first line in this node's menu
     (the line after the * Menu: line).  */
  char *menu_start;
  /* The start of the chain of sections in this node's menu.  */
  struct menu_section *sections;
  /* The last menu section in the chain.  */
  struct menu_section *last_section;
};


/* This is used for a list of sections found in a node's menu.
   Each  struct node  has such a list in the  sections  field.  */
struct menu_section
{
  struct menu_section *next;
  char *name;
  /* Line number of start of section.  */
  int start_line;
  /* Line number of end of section.  */
  int end_line;
};

/* This table defines all the long-named options, says whether they
   use an argument, and maps them into equivalent single-letter options.  */

struct option longopts[] =
{
  { "delete",    no_argument, NULL, 'r' },
  { "defentry",  required_argument, NULL, 'E' },
  { "defsection", required_argument, NULL, 'S' },
  { "dir-file",  required_argument, NULL, 'd' },
  { "entry",     required_argument, NULL, 'e' },
  { "help",      no_argument, NULL, 'h' },
  { "info-dir",  required_argument, NULL, 'D' },
  { "info-file", required_argument, NULL, 'i' },
  { "item",      required_argument, NULL, 'e' },
  { "quiet",     no_argument, NULL, 'q' },
  { "remove",    no_argument, NULL, 'r' },
  { "section",   required_argument, NULL, 's' },
  { "version",   no_argument, NULL, 'V' },
  { 0 }
};

/* Error message functions.  */

/* Print error message.  S1 is printf control string, S2 and S3 args for it. */

/* VARARGS1 */
void
error (s1, s2, s3)
     char *s1, *s2, *s3;
{
  fprintf (stderr, "%s: ", progname);
  fprintf (stderr, s1, s2, s3);
  putc ('\n', stderr);
}

/* VARARGS1 */
void
warning (s1, s2, s3)
     char *s1, *s2, *s3;
{
  fprintf (stderr, _("%s: warning: "), progname);
  fprintf (stderr, s1, s2, s3);
  putc ('\n', stderr);
}

/* Print error message and exit.  */

void
fatal (s1, s2, s3)
     char *s1, *s2, *s3;
{
  error (s1, s2, s3);
  xexit (1);
}

/* Memory allocation and string operations.  */

/* Like malloc but get fatal error if memory is exhausted.  */
void *
xmalloc (size)
     unsigned int size;
{
  extern void *malloc ();
  void *result = malloc (size);
  if (result == NULL)
    fatal (_("virtual memory exhausted"), 0);
  return result;
}

/* Like realloc but get fatal error if memory is exhausted.  */
void *
xrealloc (obj, size)
     void *obj;
     unsigned int size;
{
  extern void *realloc ();
  void *result = realloc (obj, size);
  if (result == NULL)
    fatal (_("virtual memory exhausted"), 0);
  return result;
}

/* Return a newly-allocated string
   whose contents concatenate those of S1, S2, S3.  */
char *
concat (s1, s2, s3)
     char *s1, *s2, *s3;
{
  int len1 = strlen (s1), len2 = strlen (s2), len3 = strlen (s3);
  char *result = (char *) xmalloc (len1 + len2 + len3 + 1);

  strcpy (result, s1);
  strcpy (result + len1, s2);
  strcpy (result + len1 + len2, s3);
  *(result + len1 + len2 + len3) = 0;

  return result;
}

/* Return a string containing SIZE characters
   copied from starting at STRING.  */

char *
copy_string (string, size)
     char *string;
     int size;
{
  int i;
  char *copy = (char *) xmalloc (size + 1);
  for (i = 0; i < size; i++)
    copy[i] = string[i];
  copy[size] = 0;
  return copy;
}

/* Print fatal error message based on errno, with file name NAME.  */

void
pfatal_with_name (name)
     char *name;
{
  char *s = concat ("", strerror (errno), _(" for %s"));
  fatal (s, name);
}

/* Given the full text of a menu entry, null terminated,
   return just the menu item name (copied).  */

char *
extract_menu_item_name (item_text)
     char *item_text;
{
  char *p;

  if (*item_text == '*')
    item_text++;
  while (*item_text == ' ')
    item_text++;

  p = item_text;
  while (*p && *p != ':') p++;
  return copy_string (item_text, p - item_text);
}

/* Given the full text of a menu entry, terminated by null or newline,
   return just the menu item file (copied).  */

char *
extract_menu_file_name (item_text)
     char *item_text;
{
  char *p = item_text;

  /* If we have text that looks like * ITEM: (FILE)NODE...,
     extract just FILE.  Otherwise return "(none)".  */

  if (*p == '*')
    p++;
  while (*p == ' ')
    p++;

  /* Skip to and past the colon.  */
  while (*p && *p != '\n' && *p != ':') p++;
  if (*p == ':') p++;

  /* Skip past the open-paren.  */
  while (1)
    {
      if (*p == '(')
        break;
      else if (*p == ' ' || *p == '\t')
        p++;
      else
        return "(none)";
    }
  p++;

  item_text = p;

  /* File name ends just before the close-paren.  */
  while (*p && *p != '\n' && *p != ')') p++;
  if (*p != ')')
    return "(none)";

  return copy_string (item_text, p - item_text);
}



/* Return FNAME with any [.info][.gz] suffix removed.  */

static char *
strip_info_suffix (fname)
     char *fname;
{
  char *ret = xstrdup (fname);
  unsigned len = strlen (ret);

  if (len > 3 && FILENAME_CMP (ret + len - 3, ".gz") == 0)
    {
      len -= 3;
      ret[len] = 0;
    }

  if (len > 5 && FILENAME_CMP (ret + len - 5, ".info") == 0)
    {
      len -= 5;
      ret[len] = 0;
    }
  else if (len > 4 && FILENAME_CMP (ret + len - 4, ".inf") == 0)
    {
      len -= 4;
      ret[len] = 0;
    }
#ifdef __MSDOS__
  else if (len > 4 && (FILENAME_CMP (ret + len - 4, ".inz") == 0
                       || FILENAME_CMP (ret + len - 4, ".igz") == 0))
    {
      len -= 4;
      ret[len] = 0;
    }
#endif /* __MSDOS__ */

  return ret;
}


/* Return true if ITEM matches NAME and is followed by TERM_CHAR.  ITEM
   can also be followed by `.gz', `.info.gz', or `.info' (and then
   TERM_CHAR) and still match.  */

static int
menu_item_equal (item, term_char, name)
     char *item;
     char term_char;
     char *name;
{
  unsigned name_len = strlen (name);
  /* First, ITEM must actually match NAME (usually it won't).  */
  int ret = strncasecmp (item, name, name_len) == 0;
  if (ret)
    {
      /* Then, `foobar' doesn't match `foo', so be sure we've got all of
         ITEM.  The various suffixes should never actually appear in the
         dir file, but sometimes people put them in.  */
      static char *suffixes[]
        = { "", ".info.gz", ".info", ".inf", ".gz",
#ifdef __MSDOS__
            ".inz", ".igz",
#endif
            NULL };
      unsigned i;
      ret = 0;
      for (i = 0; !ret && suffixes[i]; i++)
        {
          char *suffix = suffixes[i];
          unsigned suffix_len = strlen (suffix);
          ret = strncasecmp (item + name_len, suffix, suffix_len) == 0
                && item[name_len + suffix_len] == term_char;
        }
    }

  return ret;
}



void
suggest_asking_for_help ()
{
  fprintf (stderr, _("\tTry `%s --help' for a complete list of options.\n"),
           progname);
  xexit (1);
}

void
print_help ()
{
  printf (_("Usage: %s [OPTION]... [INFO-FILE [DIR-FILE]]\n\
\n\
Install or delete dir entries from INFO-FILE in the Info directory file\n\
DIR-FILE.\n\
\n\
Options:\n\
 --delete          delete existing entries for INFO-FILE from DIR-FILE;\n\
                     don't insert any new entries.\n\
 --defentry=TEXT   like --entry, but only use TEXT if an entry\n\
                     is not present in INFO-FILE.\n\
 --defsection=TEXT like --section, but only use TEXT if a section\n\
                     is not present in INFO-FILE.\n\
 --dir-file=NAME   specify file name of Info directory file.\n\
                     This is equivalent to using the DIR-FILE argument.\n\
 --entry=TEXT      insert TEXT as an Info directory entry.\n\
                     TEXT should have the form of an Info menu item line\n\
                     plus zero or more extra lines starting with whitespace.\n\
                     If you specify more than one entry, they are all added.\n\
                     If you don't specify any entries, they are determined\n\
                     from information in the Info file itself.\n\
 --help            display this help and exit.\n\
 --info-file=FILE  specify Info file to install in the directory.\n\
                     This is equivalent to using the INFO-FILE argument.\n\
 --info-dir=DIR    same as --dir-file=DIR/dir.\n\
 --item=TEXT       same as --entry TEXT.\n\
                     An Info directory entry is actually a menu item.\n\
 --quiet           suppress warnings.\n\
 --remove          same as --delete.\n\
 --section=SEC     put this file's entries in section SEC of the directory.\n\
                     If you specify more than one section, all the entries\n\
                     are added in each of the sections.\n\
                     If you don't specify any sections, they are determined\n\
                     from information in the Info file itself.\n\
 --version         display version information and exit.\n\
\n\
Email bug reports to bug-texinfo@gnu.org,\n\
general questions and discussion to help-texinfo@gnu.org.\n\
"), progname);
}


/* If DIRFILE does not exist, create a minimal one (or abort).  If it
   already exists, do nothing.  */

void
ensure_dirfile_exists (dirfile)
     char *dirfile;
{
  int desc = open (dirfile, O_RDONLY);
  if (desc < 0 && errno == ENOENT)
    {
      FILE *f;
      char *readerr = strerror (errno);
      close (desc);
      f = fopen (dirfile, "w");
      if (f)
        {
          fprintf (f, _("This is the file .../info/dir, which contains the\n\
topmost node of the Info hierarchy, called (dir)Top.\n\
The first time you invoke Info you start off looking at this node.\n\
\n\
%s\tThis is the top of the INFO tree\n\
\n\
  This (the Directory node) gives a menu of major topics.\n\
  Typing \"q\" exits, \"?\" lists all Info commands, \"d\" returns here,\n\
  \"h\" gives a primer for first-timers,\n\
  \"mEmacs<Return>\" visits the Emacs manual, etc.\n\
\n\
  In Emacs, you can click mouse button 2 on a menu item or cross reference\n\
  to select it.\n\
\n\
* Menu:\n\
"), "File: dir,\tNode: Top"); /* This part must not be translated.  */
          if (fclose (f) < 0)
            pfatal_with_name (dirfile);
        }
      else
        {
          /* Didn't exist, but couldn't open for writing.  */
          fprintf (stderr,
                   _("%s: could not read (%s) and could not create (%s)\n"),
                   dirfile, readerr, strerror (errno));
          xexit (1);
        }
    }
  else
    close (desc); /* It already existed, so fine.  */
}

/* Open FILENAME and return the resulting stream pointer.  If it doesn't
   exist, try FILENAME.gz.  If that doesn't exist either, call
   CREATE_CALLBACK (with FILENAME as arg) to create it, if that is
   non-NULL.  If still no luck, fatal error.

   If we do open it, return the actual name of the file opened in
   OPENED_FILENAME and the compress program to use to (de)compress it in
   COMPRESSION_PROGRAM.  The compression program is determined by the
   magic number, not the filename.  */

FILE *
open_possibly_compressed_file (filename, create_callback,
                               opened_filename, compression_program, is_pipe)
     char *filename;
     void (*create_callback) ();
     char **opened_filename;
     char **compression_program;
     int  *is_pipe;
{
  char *local_opened_filename, *local_compression_program;
  int nread;
  char data[4];
  FILE *f;

  /* We let them pass NULL if they don't want this info, but it's easier
     to always determine it.  */
  if (!opened_filename)
    opened_filename = &local_opened_filename;

  *opened_filename = filename;
  f = fopen (*opened_filename, FOPEN_RBIN);
  if (!f)
    {
      *opened_filename = concat (filename, ".gz", "");
      f = fopen (*opened_filename, FOPEN_RBIN);
#ifdef __MSDOS__
      if (!f)
        {
          free (*opened_filename);
          *opened_filename = concat (filename, ".igz", "");
          f = fopen (*opened_filename, FOPEN_RBIN);
        }
      if (!f)
        {
          free (*opened_filename);
          *opened_filename = concat (filename, ".inz", "");
          f = fopen (*opened_filename, FOPEN_RBIN);
        }
#endif
      if (!f)
        {
          if (create_callback)
            { /* That didn't work either.  Create the file if we can.  */
              (*create_callback) (filename);

              /* And try opening it again.  */
              free (*opened_filename);
              *opened_filename = filename;
              f = fopen (*opened_filename, FOPEN_RBIN);
              if (!f)
                pfatal_with_name (filename);
            }
          else
            pfatal_with_name (filename);
        }
    }

  /* Read first few bytes of file rather than relying on the filename.
     If the file is shorter than this it can't be usable anyway.  */
  nread = fread (data, sizeof (data), 1, f);
  if (nread != 1)
    {
      /* Empty files don't set errno, so we get something like
         "install-info: No error for foo", which is confusing.  */
      if (nread == 0)
        fatal (_("%s: empty file"), *opened_filename);
      pfatal_with_name (*opened_filename);
    }

  if (!compression_program)
    compression_program = &local_compression_program;

  if (data[0] == '\x1f' && data[1] == '\x8b')
#if STRIP_DOT_EXE
    /* An explicit .exe yields a better diagnostics from popen below
       if they don't have gzip installed.  */
    *compression_program = "gzip.exe";
#else
    *compression_program = "gzip";
#endif
  else
    *compression_program = NULL;

  if (*compression_program)
    { /* It's compressed, so fclose the file and then open a pipe.  */
      char *command = concat (*compression_program," -cd <", *opened_filename);
      if (fclose (f) < 0)
        pfatal_with_name (*opened_filename);
      f = popen (command, "r");
      if (f)
        *is_pipe = 1;
      else
        pfatal_with_name (command);
    }
  else
    { /* It's a plain file, seek back over the magic bytes.  */
      if (fseek (f, 0, 0) < 0)
        pfatal_with_name (*opened_filename);
#if O_BINARY
      /* Since this is a text file, and we opened it in binary mode,
         switch back to text mode.  */
      f = freopen (*opened_filename, "r", f);
#endif
      *is_pipe = 0;
    }

  return f;
}

/* Read all of file FILENAME into memory and return the address of the
   data.  Store the size of the data into SIZEP.  If need be, uncompress
   (i.e., try FILENAME.gz et al. if FILENAME does not exist) and store
   the actual file name that was opened into OPENED_FILENAME (if it is
   non-NULL), and the companion compression program (if any, else NULL)
   into COMPRESSION_PROGRAM (if that is non-NULL).  If trouble, do
   a fatal error.  */

char *
readfile (filename, sizep, create_callback,
          opened_filename, compression_program)
     char *filename;
     int *sizep;
     void (*create_callback) ();
     char **opened_filename;
     char **compression_program;
{
  char *real_name;
  FILE *f;
  int pipe_p;
  int filled = 0;
  int data_size = 8192;
  char *data = xmalloc (data_size);

  /* If they passed the space for the file name to return, use it.  */
  f = open_possibly_compressed_file (filename, create_callback,
                                     opened_filename ? opened_filename
                                                     : &real_name,
                                     compression_program, &pipe_p);

  for (;;)
    {
      int nread = fread (data + filled, 1, data_size - filled, f);
      if (nread < 0)
        pfatal_with_name (real_name);
      if (nread == 0)
        break;

      filled += nread;
      if (filled == data_size)
        {
          data_size += 65536;
          data = xrealloc (data, data_size);
        }
    }

  /* We'll end up wasting space if we're not passing the filename back
     and it is not just FILENAME, but so what.  */
  /* We need to close the stream, since on some systems the pipe created
     by popen is simulated by a temporary file which only gets removed
     inside pclose.  */
  if (pipe_p)
    pclose (f);
  else
    fclose (f);

  *sizep = filled;
  return data;
}

/* Output the old dir file, interpolating the new sections
   and/or new entries where appropriate.  If COMPRESSION_PROGRAM is not
   null, pipe to it to create DIRFILE.  Thus if we read dir.gz on input,
   we'll write dir.gz on output.  */

static void
output_dirfile (dirfile, dir_nlines, dir_lines,
                n_entries_to_add, entries_to_add, input_sections,
                compression_program)
      char *dirfile;
      int dir_nlines;
      struct line_data *dir_lines;
      int n_entries_to_add;
      struct spec_entry *entries_to_add;
      struct spec_section *input_sections;
      char *compression_program;
{
  int i;
  FILE *output;

  if (compression_program)
    {
      char *command = concat (compression_program, ">", dirfile);
      output = popen (command, "w");
    }
  else
    output = fopen (dirfile, "w");

  if (!output)
    {
      perror (dirfile);
      xexit (1);
    }

  for (i = 0; i <= dir_nlines; i++)
    {
      int j;

      /* If we decided to output some new entries before this line,
         output them now.  */
      if (dir_lines[i].add_entries_before)
        for (j = 0; j < n_entries_to_add; j++)
          {
            struct spec_entry *this = dir_lines[i].add_entries_before[j];
            if (this == 0)
              break;
            fputs (this->text, output);
          }
      /* If we decided to add some sections here
         because there are no such sections in the file,
         output them now.  */
      if (dir_lines[i].add_sections_before)
        {
          struct spec_section *spec;
          struct spec_section **sections;
          int n_sections = 0;
          struct spec_entry *entry;
          struct spec_entry **entries;
          int n_entries = 0;

          /* Count the sections and allocate a vector for all of them.  */
          for (spec = input_sections; spec; spec = spec->next)
            n_sections++;
          sections = ((struct spec_section **)
                      xmalloc (n_sections * sizeof (struct spec_section *)));

          /* Fill the vector SECTIONS with pointers to all the sections,
             and sort them.  */
          j = 0;
          for (spec = input_sections; spec; spec = spec->next)
            sections[j++] = spec;
          qsort (sections, n_sections, sizeof (struct spec_section *),
                 compare_section_names);

          /* Count the entries and allocate a vector for all of them.  */
          for (entry = entries_to_add; entry; entry = entry->next)
            n_entries++;
          entries = ((struct spec_entry **)
                     xmalloc (n_entries * sizeof (struct spec_entry *)));

          /* Fill the vector ENTRIES with pointers to all the sections,
             and sort them.  */
          j = 0;
          for (entry = entries_to_add; entry; entry = entry->next)
            entries[j++] = entry;
          qsort (entries, n_entries, sizeof (struct spec_entry *),
                 compare_entries_text);

          /* Generate the new sections in alphabetical order.  In each
             new section, output all of the entries that belong to that
             section, in alphabetical order.  */
          for (j = 0; j < n_sections; j++)
            {
              spec = sections[j];
              if (spec->missing)
                {
                  int k;

                  putc ('\n', output);
                  fputs (spec->name, output);
                  putc ('\n', output);
                  for (k = 0; k < n_entries; k++)
                    {
                      struct spec_section *spec1;
                      /* Did they at all want this entry to be put into
                         this section?  */
                      entry = entries[k];
                      for (spec1 = entry->entry_sections;
                           spec1 && spec1 != entry->entry_sections_tail;
                           spec1 = spec1->next)
                        {
                          if (!strcmp (spec1->name, spec->name))
                            break;
                        }
                      if (spec1 && spec1 != entry->entry_sections_tail)
                        fputs (entry->text, output);
                    }
                }
            }

          free (entries);
          free (sections);
        }

      /* Output the original dir lines unless marked for deletion.  */
      if (i < dir_nlines && !dir_lines[i].delete)
        {
          fwrite (dir_lines[i].start, 1, dir_lines[i].size, output);
          putc ('\n', output);
        }
    }

  /* Some systems, such as MS-DOS, simulate pipes with temporary files.
     On those systems, the compressor actually gets run inside pclose,
     so we must call pclose.  */
  if (compression_program)
    pclose (output);
  else
    fclose (output);
}

/* Parse the input to find the section names and the entry names it
   specifies.  Return the number of entries to add from this file.  */
int
parse_input (lines, nlines, sections, entries)
     const struct line_data *lines;
     int nlines;
     struct spec_section **sections;
     struct spec_entry **entries;
{
  int n_entries = 0;
  int prefix_length = strlen ("INFO-DIR-SECTION ");
  struct spec_section *head = *sections, *tail = NULL;
  int reset_tail = 0;
  char *start_of_this_entry = 0;
  int ignore_sections = *sections != 0;
  int ignore_entries  = *entries  != 0;

  int i;

  if (ignore_sections && ignore_entries)
    return 0;

  /* Loop here processing lines from the input file.  Each
     INFO-DIR-SECTION entry is added to the SECTIONS linked list.
     Each START-INFO-DIR-ENTRY block is added to the ENTRIES linked
     list, and all its entries inherit the chain of SECTION entries
     defined by the last group of INFO-DIR-SECTION entries we have
     seen until that point.  */
  for (i = 0; i < nlines; i++)
    {
      if (!ignore_sections
          && !strncmp ("INFO-DIR-SECTION ", lines[i].start, prefix_length))
        {
          struct spec_section *next
            = (struct spec_section *) xmalloc (sizeof (struct spec_section));
          next->name = copy_string (lines[i].start + prefix_length,
                                    lines[i].size - prefix_length);
          next->next = *sections;
          next->missing = 1;
          if (reset_tail)
            {
              tail = *sections;
              reset_tail = 0;
            }
          *sections = next;
          head = *sections;
        }
      /* If entries were specified explicitly with command options,
         ignore the entries in the input file.  */
      else if (!ignore_entries)
        {
          if (!strncmp ("START-INFO-DIR-ENTRY", lines[i].start, lines[i].size)
              && sizeof ("START-INFO-DIR-ENTRY") - 1 == lines[i].size)
            {
              if (!*sections)
                {
                  /* We found an entry, but didn't yet see any sections
                     specified.  Default to section "Miscellaneous".  */
                  *sections = (struct spec_section *)
                    xmalloc (sizeof (struct spec_section));
                  (*sections)->name =
		    default_section ? default_section : "Miscellaneous";
                  (*sections)->next = 0;
                  (*sections)->missing = 1;
                  head = *sections;
                }
              /* Next time we see INFO-DIR-SECTION, we will reset the
                 tail pointer.  */
              reset_tail = 1;

              if (start_of_this_entry != 0)
                fatal (_("START-INFO-DIR-ENTRY without matching END-INFO-DIR-ENTRY"));
              start_of_this_entry = lines[i + 1].start;
            }
          else if (start_of_this_entry)
            {
              if ((!strncmp ("* ", lines[i].start, 2)
                   && lines[i].start > start_of_this_entry)
                  || (!strncmp ("END-INFO-DIR-ENTRY",
                                lines[i].start, lines[i].size)
                      && sizeof ("END-INFO-DIR-ENTRY") - 1 == lines[i].size))
                {
                  /* We found an end of this entry.  Allocate another
                     entry, fill its data, and add it to the linked
                     list.  */
                  struct spec_entry *next
                    = (struct spec_entry *) xmalloc (sizeof (struct spec_entry));
                  next->text
                    = copy_string (start_of_this_entry,
                                   lines[i].start - start_of_this_entry);
                  next->text_len = lines[i].start - start_of_this_entry;
                  next->entry_sections = head;
                  next->entry_sections_tail = tail;
                  next->next = *entries;
                  *entries = next;
                  n_entries++;
                  if (!strncmp ("END-INFO-DIR-ENTRY",
                                lines[i].start, lines[i].size)
                      && sizeof ("END-INFO-DIR-ENTRY") - 1 == lines[i].size)
                    start_of_this_entry = 0;
                  else
                    start_of_this_entry = lines[i].start;
                }
              else if (!strncmp ("END-INFO-DIR-ENTRY",
                                 lines[i].start, lines[i].size)
                       && sizeof ("END-INFO-DIR-ENTRY") - 1 == lines[i].size)
                fatal (_("END-INFO-DIR-ENTRY without matching START-INFO-DIR-ENTRY"));
            }
        }
    }
  if (start_of_this_entry != 0)
    fatal (_("START-INFO-DIR-ENTRY without matching END-INFO-DIR-ENTRY"));

  /* If we ignored the INFO-DIR-ENTRY directives, we need now go back
     and plug the names of all the sections we found into every
     element of the ENTRIES list.  */
  if (ignore_entries && *entries)
    {
      struct spec_entry *entry;

      for (entry = *entries; entry; entry = entry->next)
        {
          entry->entry_sections = head;
          entry->entry_sections_tail = tail;
        }
    }

  return n_entries;
}

/* Parse the dir file whose basename is BASE_NAME.  Find all the
   nodes, and their menus, and the sections of their menus.  */
int
parse_dir_file (lines, nlines, nodes, base_name)
     struct line_data *lines;
     int nlines;
     struct node **nodes;
     const char *base_name;
{
  int node_header_flag = 0;
  int something_deleted = 0;
  int i;

  *nodes = 0;
  for (i = 0; i < nlines; i++)
    {
      /* Parse node header lines.  */
      if (node_header_flag)
        {
          int j, end;
          for (j = 0; j < lines[i].size; j++)
            /* Find the node name and store it in the `struct node'.  */
            if (!strncmp ("Node:", lines[i].start + j, 5))
              {
                char *line = lines[i].start;
                /* Find the start of the node name.  */
                j += 5;
                while (line[j] == ' ' || line[j] == '\t')
                  j++;
                /* Find the end of the node name.  */
                end = j;
                while (line[end] != 0 && line[end] != ',' && line[end] != '\n'
                       && line[end] != '\t')
                  end++;
                (*nodes)->name = copy_string (line + j, end - j);
              }
          node_header_flag = 0;
        }

      /* Notice the start of a node.  */
      if (*lines[i].start == 037)
        {
          struct node *next = (struct node *) xmalloc (sizeof (struct node));

          next->next = *nodes;
          next->name = NULL;
          next->start_line = i;
          next->end_line = 0;
          next->menu_start = NULL;
          next->sections = NULL;
          next->last_section = NULL;

          if (*nodes != 0)
            (*nodes)->end_line = i;
          /* Fill in the end of the last menu section
             of the previous node.  */
          if (*nodes != 0 && (*nodes)->last_section != 0)
            (*nodes)->last_section->end_line = i;

          *nodes = next;

          /* The following line is the header of this node;
             parse it.  */
          node_header_flag = 1;
        }

      /* Notice the lines that start menus.  */
      if (*nodes != 0 && !strncmp ("* Menu:", lines[i].start, 7))
        (*nodes)->menu_start = lines[i + 1].start;

      /* Notice sections in menus.  */
      if (*nodes != 0
          && (*nodes)->menu_start != 0
          && *lines[i].start != '\n'
          && *lines[i].start != '*'
          && *lines[i].start != ' '
          && *lines[i].start != '\t')
        {
          /* Add this menu section to the node's list.
             This list grows in forward order.  */
          struct menu_section *next
            = (struct menu_section *) xmalloc (sizeof (struct menu_section));

          next->start_line = i + 1;
          next->next = 0;
          next->end_line = 0;
          next->name = copy_string (lines[i].start, lines[i].size);
          if ((*nodes)->sections)
            {
              (*nodes)->last_section->next = next;
              (*nodes)->last_section->end_line = i;
            }
          else
            (*nodes)->sections = next;
          (*nodes)->last_section = next;
        }

      /* Check for an existing entry that should be deleted.
         Delete all entries which specify this file name.  */
      if (*lines[i].start == '*')
        {
          char *q;
          char *p = lines[i].start;

          p++; /* skip * */
          while (*p == ' ') p++; /* ignore following spaces */
          q = p; /* remember this, it's the beginning of the menu item.  */

          /* Read menu item.  */
          while (*p != 0 && *p != ':')
            p++;
          p++; /* skip : */

          if (*p == ':')
            { /* XEmacs-style entry, as in * Mew::Messaging.  */
              if (menu_item_equal (q, ':', base_name))
                {
                  lines[i].delete = 1;
                  something_deleted = 1;
                }
            }
          else
            { /* Emacs-style entry, as in * Emacs: (emacs).  */
              while (*p == ' ') p++; /* skip spaces after : */
              if (*p == '(')         /* if at parenthesized (FILENAME) */
                {
                  p++;
                  if (menu_item_equal (p, ')', base_name))
                    {
                      lines[i].delete = 1;
                      something_deleted = 1;
                    }
                }
            }
        }

      /* Treat lines that start with whitespace
         as continuations; if we are deleting an entry,
         delete all its continuations as well.  */
      else if (i > 0 && (*lines[i].start == ' ' || *lines[i].start == '\t'))
        {
          lines[i].delete = lines[i - 1].delete;
        }
    }

  /* Finish the info about the end of the last node.  */
  if (*nodes != 0)
    {
      (*nodes)->end_line = nlines;
      if ((*nodes)->last_section != 0)
        (*nodes)->last_section->end_line = nlines;
    }

  return something_deleted;
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  char *opened_dirfilename;
  char *compression_program;
  char *infile_sans_info;
  char *infile = 0, *dirfile = 0;
  unsigned infilelen_sans_info;

  /* Record the text of the Info file, as a sequence of characters
     and as a sequence of lines.  */
  char *input_data = NULL;
  int input_size = 0;
  struct line_data *input_lines = NULL;
  int input_nlines = 0;

  /* Record here the specified section names and directory entries.  */
  struct spec_section *input_sections = NULL;
  struct spec_entry *entries_to_add = NULL;
  int n_entries_to_add = 0;
  struct spec_entry *default_entries_to_add = NULL;
  int n_default_entries_to_add = 0;

  /* Record the old text of the dir file, as plain characters,
     as lines, and as nodes.  */
  char *dir_data;
  int dir_size;
  int dir_nlines;
  struct line_data *dir_lines;
  struct node *dir_nodes;

  /* Nonzero means --delete was specified (just delete existing entries).  */
  int delete_flag = 0;
  int something_deleted = 0;
  /* Nonzero means -q was specified.  */
  int quiet_flag = 0;

  int i;

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  while (1)
    {
      int opt = getopt_long (argc, argv, "i:d:e:s:hHr", longopts, 0);

      if (opt == EOF)
        break;

      switch (opt)
        {
        case 0:
          /* If getopt returns 0, then it has already processed a
             long-named option.  We should do nothing.  */
          break;

        case 1:
          abort ();

        case 'd':
          if (dirfile)
            {
              fprintf (stderr, _("%s: Specify the Info directory only once.\n"),
                       progname);
              suggest_asking_for_help ();
            }
          dirfile = optarg;
          break;

        case 'D':
          if (dirfile)
            {
              fprintf (stderr, _("%s: Specify the Info directory only once.\n"),
                       progname);
              suggest_asking_for_help ();
            }
          dirfile = concat (optarg, "", "/dir");
          break;

	case 'E':
        case 'e':
          {
            struct spec_entry *next
              = (struct spec_entry *) xmalloc (sizeof (struct spec_entry));
            int olen = strlen (optarg);
            if (! (*optarg != 0 && optarg[olen - 1] == '\n'))
              {
                optarg = concat (optarg, "\n", "");
                olen++;
              }
            next->text = optarg;
            next->text_len = olen;
            next->entry_sections = NULL;
            next->entry_sections_tail = NULL;
	    if (opt == 'e')
	      {
		next->next = entries_to_add;
		entries_to_add = next;
		n_entries_to_add++;
	      }
	    else
	      {
		next->next = default_entries_to_add;
		default_entries_to_add = next;
		n_default_entries_to_add++;
	      }
          }
          break;

        case 'h':
        case 'H':
          print_help ();
          xexit (0);

        case 'i':
          if (infile)
            {
              fprintf (stderr, _("%s: Specify the Info file only once.\n"),
                       progname);
              suggest_asking_for_help ();
            }
          infile = optarg;
          break;

        case 'q':
          quiet_flag = 1;
          break;

        case 'r':
          delete_flag = 1;
          break;

        case 's':
          {
            struct spec_section *next
              = (struct spec_section *) xmalloc (sizeof (struct spec_section));
            next->name = optarg;
            next->next = input_sections;
            next->missing = 1;
            input_sections = next;
          }
          break;

	case 'S':
	  default_section = optarg;
	  break;

        case 'V':
          printf ("install-info (GNU %s) %s\n", PACKAGE, VERSION);
          puts ("");
	  printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
There is NO warranty.  You may redistribute this software\n\
under the terms of the GNU General Public License.\n\
For more information about these matters, see the files named COPYING.\n"),
		  "1999");
          xexit (0);

        default:
          suggest_asking_for_help ();
        }
    }

  /* Interpret the non-option arguments as file names.  */
  for (; optind < argc; ++optind)
    {
      if (infile == 0)
        infile = argv[optind];
      else if (dirfile == 0)
        dirfile = argv[optind];
      else
        error (_("excess command line argument `%s'"), argv[optind]);
    }

  if (!infile)
    fatal (_("No input file specified; try --help for more information."));
  if (!dirfile)
    fatal (_("No dir file specified; try --help for more information."));

  /* Read the Info file and parse it into lines, unless we're deleting.  */
  if (!delete_flag)
    {
      input_data = readfile (infile, &input_size, NULL, NULL, NULL);
      input_lines = findlines (input_data, input_size, &input_nlines);
    }

  i = parse_input (input_lines, input_nlines,
                   &input_sections, &entries_to_add);
  if (i > n_entries_to_add)
    n_entries_to_add = i;
  else if (n_entries_to_add == 0)
    {
      entries_to_add = default_entries_to_add;
      n_entries_to_add = n_default_entries_to_add;
    }

  if (!delete_flag)
    {
      if (entries_to_add == 0)
        { /* No need to abort here, the original info file may not
             have the requisite Texinfo commands.  This is not
             something an installer should have to correct (it's a
             problem for the maintainer), and there's no need to cause
             subsequent parts of `make install' to fail.  */
          warning (_("no info dir entry in `%s'"), infile);
          xexit (0);
        }

      /* If the entries came from the command-line arguments, their
         entry_sections pointers are not yet set.  Walk the chain of
         the entries and for each entry update entry_sections to point
         to the head of the list of sections where this entry should
         be put.  Note that all the entries specified on the command
         line get put into ALL the sections we've got, either from the
         Info file, or (under --section) from the command line,
         because in the loop below every entry inherits the entire
         chain of sections.  */
      if (n_entries_to_add > 0 && entries_to_add->entry_sections == NULL)
        {
          struct spec_entry *ep;

          /* If we got no sections, default to "Miscellaneous".  */
          if (input_sections == NULL)
            {
              input_sections = (struct spec_section *)
                xmalloc (sizeof (struct spec_section));
              input_sections->name =
		default_section ? default_section : "Miscellaneous";
              input_sections->next = NULL;
              input_sections->missing = 1;
            }
          for (ep = entries_to_add; ep; ep = ep->next)
            ep->entry_sections = input_sections;
        }
    }

  /* Now read in the Info dir file.  */
  dir_data = readfile (dirfile, &dir_size, ensure_dirfile_exists,
                       &opened_dirfilename, &compression_program);
  dir_lines = findlines (dir_data, dir_size, &dir_nlines);

  /* We will be comparing the entries in the dir file against the
     current filename, so need to strip off any directory prefix and/or
     [.info][.gz] suffix.  */
  {
    char *infile_basename = infile + strlen (infile);

    if (HAVE_DRIVE (infile))
      infile += 2;	/* get past the drive spec X: */

    while (infile_basename > infile && !IS_SLASH (infile_basename[-1]))
      infile_basename--;

    infile_sans_info = strip_info_suffix (infile_basename);
    infilelen_sans_info = strlen (infile_sans_info);
  }

  something_deleted
    = parse_dir_file (dir_lines, dir_nlines, &dir_nodes, infile_sans_info);

  /* Decide where to add the new entries (unless --delete was used).
     Find the menu sections to add them in.
     In each section, find the proper alphabetical place to add
     each of the entries.  */

  if (!delete_flag)
    {
      struct node *node;
      struct menu_section *section;
      struct spec_section *spec;

      for (node = dir_nodes; node; node = node->next)
        for (section = node->sections; section; section = section->next)
          {
            for (i = section->end_line; i > section->start_line; i--)
              if (dir_lines[i - 1].size != 0)
                break;
            section->end_line = i;

            for (spec = input_sections; spec; spec = spec->next)
              if (!strcmp (spec->name, section->name))
                break;
            if (spec)
              {
                int add_at_line = section->end_line;
                struct spec_entry *entry;
                /* Say we have found at least one section with this name,
                   so we need not add such a section.  */
                spec->missing = 0;
                /* For each entry, find the right place in this section
                   to add it.  */
                for (entry = entries_to_add; entry; entry = entry->next)
                  {
                    /* Did they at all want this entry to be put into
                       this section?  */
                    for (spec = entry->entry_sections;
                         spec && spec != entry->entry_sections_tail;
                         spec = spec->next)
                      {
                        if (!strcmp (spec->name, section->name))
                          break;
                      }
                    if (!spec || spec == entry->entry_sections_tail)
                      continue;
                    
                    /* Subtract one because dir_lines is zero-based,
                       but the `end_line' and `start_line' members are
                       one-based.  */
                    for (i = section->end_line - 1;
                         i >= section->start_line - 1; i--)
                      {
                        /* If an entry exists with the same name,
                           and was not marked for deletion
                           (which means it is for some other file),
                           we are in trouble.  */
                        if (dir_lines[i].start[0] == '*'
                            && menu_line_equal (entry->text, entry->text_len,
                                                dir_lines[i].start,
                                                dir_lines[i].size)
                            && !dir_lines[i].delete)
			  {
			    if (quiet_flag)
			      dir_lines[i].delete = 1;
			    else
			      fatal (_("menu item `%s' already exists, for file `%s'"),
                                 extract_menu_item_name (entry->text),
                                 extract_menu_file_name (dir_lines[i].start));
			  }
                        if (dir_lines[i].start[0] == '*'
                            && menu_line_lessp (entry->text, entry->text_len,
                                                dir_lines[i].start,
                                                dir_lines[i].size))
                          add_at_line = i;
                      }
                    insert_entry_here (entry, add_at_line,
                                       dir_lines, n_entries_to_add);
                  }
              }
          }

      /* Mark the end of the Top node as the place to add any
         new sections that are needed.  */
      for (node = dir_nodes; node; node = node->next)
        if (node->name && strcmp (node->name, "Top") == 0)
          dir_lines[node->end_line].add_sections_before = 1;
    }

  if (delete_flag && !something_deleted && !quiet_flag)
    warning (_("no entries found for `%s'; nothing deleted"), infile);

  output_dirfile (opened_dirfilename, dir_nlines, dir_lines, n_entries_to_add,
                  entries_to_add, input_sections, compression_program);

  xexit (0);
}

/* Divide the text at DATA (of SIZE bytes) into lines.
   Return a vector of struct line_data describing the lines.
   Store the length of that vector into *NLINESP.  */

struct line_data *
findlines (data, size, nlinesp)
     char *data;
     int size;
     int *nlinesp;
{
  int i;
  int lineflag = 1;
  int lines_allocated = 511;
  int filled = 0;
  struct line_data *lines
    = xmalloc ((lines_allocated + 1) * sizeof (struct line_data));

  for (i = 0; i < size; i++)
    {
      if (lineflag)
        {
          if (filled == lines_allocated)
            {
              /* try to keep things somewhat page-aligned */
              lines_allocated = ((lines_allocated + 1) * 2) - 1;
              lines = xrealloc (lines, (lines_allocated + 1)
                                       * sizeof (struct line_data));
            }
          lines[filled].start = &data[i];
          lines[filled].add_entries_before = 0;
          lines[filled].add_sections_before = 0;
          lines[filled].delete = 0;
          if (filled > 0)
            lines[filled - 1].size
              = lines[filled].start - lines[filled - 1].start - 1;
          filled++;
        }
      lineflag = (data[i] == '\n');
    }
  if (filled > 0)
    lines[filled - 1].size = &data[i] - lines[filled - 1].start - lineflag;

  /* Do not leave garbage in the last element.  */
  lines[filled].start = NULL;
  lines[filled].add_entries_before = NULL;
  lines[filled].add_sections_before = 0;
  lines[filled].delete = 0;
  lines[filled].size = 0;

  *nlinesp = filled;
  return lines;
}

/* Compare the menu item names in LINE1 (line length LEN1)
   and LINE2 (line length LEN2).  Return 1 if the item name
   in LINE1 is less, 0 otherwise.  */

int
menu_line_lessp (line1, len1, line2, len2)
     char *line1;
     int len1;
     char *line2;
     int len2;
{
  int minlen = (len1 < len2 ? len1 : len2);
  int i;

  for (i = 0; i < minlen; i++)
    {
      /* If one item name is a prefix of the other,
         the former one is less.  */
      if (line1[i] == ':' && line2[i] != ':')
        return 1;
      if (line2[i] == ':' && line1[i] != ':')
        return 0;
      /* If they both continue and differ, one is less.  */
      if (line1[i] < line2[i])
        return 1;
      if (line1[i] > line2[i])
        return 0;
    }
  /* With a properly formatted dir file,
     we can only get here if the item names are equal.  */
  return 0;
}

/* Compare the menu item names in LINE1 (line length LEN1)
   and LINE2 (line length LEN2).  Return 1 if the item names are equal,
   0 otherwise.  */

int
menu_line_equal (line1, len1, line2, len2)
     char *line1;
     int len1;
     char *line2;
     int len2;
{
  int minlen = (len1 < len2 ? len1 : len2);
  int i;

  for (i = 0; i < minlen; i++)
    {
      /* If both item names end here, they are equal.  */
      if (line1[i] == ':' && line2[i] == ':')
        return 1;
      /* If they both continue and differ, one is less.  */
      if (line1[i] != line2[i])
        return 0;
    }
  /* With a properly formatted dir file,
     we can only get here if the item names are equal.  */
  return 1;
}

/* This is the comparison function for qsort
   for a vector of pointers to struct spec_section.
   Compare the section names.  */

int
compare_section_names (sec1, sec2)
     struct spec_section **sec1, **sec2;
{
  char *name1 = (*sec1)->name;
  char *name2 = (*sec2)->name;
  return strcmp (name1, name2);
}

/* This is the comparison function for qsort
   for a vector of pointers to struct spec_entry.
   Compare the entries' text.  */

int
compare_entries_text (entry1, entry2)
     struct spec_entry **entry1, **entry2;
{
  char *text1 = (*entry1)->text;
  char *text2 = (*entry2)->text;
  char *colon1 = strchr (text1, ':');
  char *colon2 = strchr (text2, ':');
  int len1, len2;

  if (!colon1)
    len1 = strlen (text1);
  else
    len1 = colon1 - text1;
  if (!colon2)
    len2 = strlen (text2);
  else
    len2 = colon2 - text2;
  return strncmp (text1, text2, len1 <= len2 ? len1 : len2);
}

/* Insert ENTRY into the add_entries_before vector
   for line number LINE_NUMBER of the dir file.
   DIR_LINES and N_ENTRIES carry information from like-named variables
   in main.  */

void
insert_entry_here (entry, line_number, dir_lines, n_entries)
     struct spec_entry *entry;
     int line_number;
     struct line_data *dir_lines;
     int n_entries;
{
  int i, j;

  if (dir_lines[line_number].add_entries_before == 0)
    {
      dir_lines[line_number].add_entries_before
        = (struct spec_entry **) xmalloc (n_entries * sizeof (struct spec_entry *));
      for (i = 0; i < n_entries; i++)
        dir_lines[line_number].add_entries_before[i] = 0;
    }

  /* Find the place where this entry belongs.  If there are already
     several entries to add before LINE_NUMBER, make sure they are in
     alphabetical order.  */
  for (i = 0; i < n_entries; i++)
    if (dir_lines[line_number].add_entries_before[i] == 0
        || menu_line_lessp (entry->text, strlen (entry->text),
                            dir_lines[line_number].add_entries_before[i]->text,
                            strlen (dir_lines[line_number].add_entries_before[i]->text)))
      break;

  if (i == n_entries)
    abort ();

  /* If we need to plug ENTRY into the middle of the
     ADD_ENTRIES_BEFORE array, move the entries which should be output
     after this one down one notch, before adding a new one.  */
  if (dir_lines[line_number].add_entries_before[i] != 0)
    for (j = n_entries - 1; j > i; j--)
      dir_lines[line_number].add_entries_before[j]
        = dir_lines[line_number].add_entries_before[j - 1];

  dir_lines[line_number].add_entries_before[i] = entry;
}
