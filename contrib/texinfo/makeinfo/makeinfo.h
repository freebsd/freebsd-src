/* makeinfo.h -- declarations for Makeinfo.
   $Id: makeinfo.h,v 1.31 2001/09/11 16:37:51 karl Exp $

   Copyright (C) 1996, 97, 98, 99, 2000, 01 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#ifndef MAKEINFO_H
#define MAKEINFO_H

#ifdef COMPILING_MAKEINFO
#  define DECLARE(type,var,init) type var = init
#else
#  define DECLARE(type,var,init) extern type var
#endif

/* Hardcoded per GNU standards, not dependent on argv[0].  */
DECLARE (char *, progname, "makeinfo");

enum reftype
{
  menu_reference, followed_reference
};

extern char *get_xref_token ();

/* Nonzero means a string is in execution, as opposed to a file. */
DECLARE (int, executing_string, 0);

/* Nonzero means to inhibit writing macro expansions to the output
   stream, because it has already been written. */
DECLARE (int, me_inhibit_expansion, 0);

extern char *expansion (), *text_expansion ();

/* Current output stream. */
DECLARE (FILE *, output_stream, NULL);

DECLARE (char *, pretty_output_filename, NULL);

/* Current output file name.  */
DECLARE (char *, current_output_filename, NULL);

/* Output paragraph buffer. */
DECLARE (unsigned char *, output_paragraph, NULL);

/* Offset into OUTPUT_PARAGRAPH. */
DECLARE (int, output_paragraph_offset, 0);

/* The output paragraph "cursor" horizontal position. */
DECLARE (int, output_column, 0);

/* Position in the output file. */
DECLARE (int, output_position, 0);

/* The offset into OUTPUT_PARAGRAPH where we have a meta character
   produced by a markup such as @code or @dfn.  */
DECLARE (int, meta_char_pos, -1);

/* Nonzero means output_paragraph contains text. */
DECLARE (int, paragraph_is_open, 0);

/* Nonzero means that `start_paragraph' MUST be called before we pay
   any attention to `close_paragraph' calls. */
DECLARE (int, must_start_paragraph, 0);

/* Nonzero means that we have seen "@top" once already. */
DECLARE (int, top_node_seen, 0);

/* Nonzero means that we have seen a non-"@top" node already. */
DECLARE (int, non_top_node_seen, 0);

/* Nonzero indicates that indentation is temporarily turned off. */
DECLARE (int, no_indent, 1);

/* Nonzero indicates that filling a line also indents the new line. */
DECLARE (int, indented_fill, 0);

/* Nonzero means forcing output text to be flushright. */
DECLARE (int, force_flush_right, 0);

/* The amount of indentation to apply at the start of each line. */
DECLARE (int, current_indent, 0);

/* The column at which long lines are broken. */
DECLARE (int, fill_column, 72);

/* Nonzero means that words are not to be split, even in long lines.  This
   gets changed for cm_w (). */
DECLARE (int, non_splitting_words, 0);

/* Amount by which @example indentation increases/decreases. */
DECLARE (int, default_indentation_increment, 5);

/* Nonzero means that we are currently hacking the insides of an
   insertion which would use a fixed width font. */
DECLARE (int, in_fixed_width_font, 0);

/* Nonzero if we are currently processing a multitable command */
DECLARE (int, multitable_active, 0);

/* Nonzero means that we're generating HTML. */
DECLARE (int, html, 0);

/* Nonzero means that we're generating XML. */
DECLARE (int, xml, 0);

/* Nonzero means that we're generating DocBook. */
DECLARE (int, docbook, 0);

/* Nonzero means true 8-bit output for Info and plain text.  */
DECLARE (int, enable_encoding, 0);

/* Nonzero means escape characters in HTML output. */
DECLARE (int, escape_html, 1);
extern char *escape_string (); /* do HTML escapes */

/* Nonzero means that the use of paragraph_start_indent is inhibited.
   @example uses this to line up the left columns of the example text.
   A negative value for this variable is incremented each time it is used.
   @noindent uses this to inhibit indentation for a single paragraph.  */
DECLARE (int, inhibit_paragraph_indentation, 0);

/* Nonzero indicates that filling will take place on long lines. */
DECLARE (int, filling_enabled, 1);

/* The current node's node name. */
DECLARE (char *, current_node, NULL);

/* Command name in the process of being hacked. */
DECLARE (char *, command, NULL);

/* @documentdescription ... @end documentdescription. */
DECLARE (char *, document_description, NULL);

/* Nonzero if the last character inserted has the syntax class of NEWLINE. */
DECLARE (int, last_char_was_newline, 1);

/* The current input file state. */
DECLARE (char *, input_filename, (char *)NULL);
DECLARE (char *, input_text, (char *)NULL);
DECLARE (int, input_text_length, 0);
DECLARE (int, input_text_offset, 0);
DECLARE (int, line_number, 0);
DECLARE (char *, toplevel_output_filename, NULL);
#define curchar() input_text[input_text_offset]

/* A colon separated list of directories to search for files included
   with @include.  This can be controlled with the `-I' option to makeinfo. */
DECLARE (char *, include_files_path, NULL);

/* The filename of the current input file.  This is never freed. */
DECLARE (char *, node_filename, NULL);

/* Nonzero means do not output "Node: Foo" for node separations, that
   is, generate plain text.  (--no-headers) */
DECLARE (int, no_headers, 0);

/* Nonzero means that we process @html and @rawhtml even when not
   generating HTML.  (--ifhtml) */
DECLARE (int, process_html, 0);

/* Nonzero means that we process @ifinfo even when generating HTML.
   (--ifinfo) */
DECLARE (int, process_info, 1);

/* Nonzero means that we process @tex and @iftex.  (--iftex) */
DECLARE (int, process_tex, 0);

/* Maximum number of references to a single node before complaining.
   (--reference-limit) */
DECLARE (int, reference_warning_limit, 1000);

/* Default is to check node references.  (--no-validate) */
DECLARE (int, validating, 1);

/* Nonzero means print information about what is going on.  (--verbose) */
DECLARE (int, verbose_mode, 0);

/* Nonzero means prefix each @chapter, ... with a number like 1. (--number-sections) */
DECLARE (int, number_sections, 0);

/* Nonzero means split size.  When zero, DEFAULT_SPLIT_SIZE is used. */
DECLARE (int, split_size, 0);

/* Nonzero means expand node names and references while validating.
   This will avoid errors when the Texinfo document uses features
   like @@ and @value inconsistently in node names, but will slow
   the program by about 80%.  You HAVE been warned.  */
DECLARE (int, expensive_validation, 0);

/* C's standard macros don't check to make sure that the characters being
   changed are within range.  So I have to check explicitly. */

#define coerce_to_upper(c) ((islower(c) ? toupper(c) : (c)))
#define coerce_to_lower(c) ((isupper(c) ? tolower(c) : (c)))

#define control_character_bit 0x40 /* %01000000, must be off. */
#define meta_character_bit 0x080/* %10000000, must be on.  */
#define CTL(c) ((c) & (~control_character_bit))
#define UNCTL(c) coerce_to_upper(((c)|control_character_bit))
#define META(c) ((c) | (meta_character_bit))
#define UNMETA(c) ((c) & (~meta_character_bit))

#define whitespace(c)       ((c) == '\t' || (c) == ' ')
#define sentence_ender(c)   ((c) == '.'  || (c) == '?' || (c) == '!')
#define cr_or_whitespace(c) (whitespace(c) || (c) == '\r' || (c) == '\n')

#ifndef isletter
#define isletter(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#endif

#ifndef isupper
#define isupper(c) ((c) >= 'A' && (c) <= 'Z')
#endif

#ifndef isdigit
#define isdigit(c)  ((c) >= '0' && (c) <= '9')
#endif

#ifndef digit_value
#define digit_value(c) ((c) - '0')
#endif

#define HTML_SAFE "$-_.+!*'()"
#define URL_SAFE_CHAR(ch) (isalnum (ch) || strchr (HTML_SAFE, ch))

#define COMMAND_PREFIX '@'

#define END_VERBATIM "end verbatim"

/* Stuff for splitting large files. */
#define SPLIT_SIZE_THRESHOLD 70000  /* What's good enough for Stallman... */
#define DEFAULT_SPLIT_SIZE 50000    /* Is probably good enough for me. */
DECLARE (int, splitting, 1);    /* Defaults to true for now. */

#define command_char(c) (!cr_or_whitespace(c) \
                         && (c) != '{' \
                         && (c) != '}' \
                         && (c) != '=')

#define skip_whitespace() \
     while ((input_text_offset != input_text_length) && \
             whitespace (curchar())) \
       input_text_offset++

#define skip_whitespace_and_newlines() \
  do { \
   while (input_text_offset != input_text_length \
          && cr_or_whitespace (curchar ())) \
      { \
         if (curchar () == '\n') \
           line_number++; \
         input_text_offset++; \
      } \
   } while (0)

/* Return nonzero if STRING is the text at input_text + input_text_offset,
   else zero. */
#define looking_at(string) \
  (strncmp (input_text + input_text_offset, string, strlen (string)) == 0)

#endif /* not MAKEINFO_H */
