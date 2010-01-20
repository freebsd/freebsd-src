/* insertion.c -- insertions for Texinfo.
   $Id: insertion.c,v 1.55 2004/11/11 18:34:28 karl Exp $

   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"
#include "cmds.h"
#include "defun.h"
#include "float.h"
#include "html.h"
#include "insertion.h"
#include "macro.h"
#include "makeinfo.h"
#include "multi.h"
#include "xml.h"

/* Must match list in insertion.h.  */
static char *insertion_type_names[] =
{ 
  "cartouche", "copying", "defcv", "deffn", "defivar", "defmac",
  "defmethod", "defop", "defopt", "defspec", "deftp", "deftypecv",
  "deftypefn", "deftypefun", "deftypeivar", "deftypemethod",
  "deftypeop", "deftypevar", "deftypevr", "defun", "defvar", "defvr",
  "detailmenu", "direntry", "display", "documentdescription",
  "enumerate", "example", "float", "flushleft", "flushright", "format",
  "ftable", "group", "ifclear", "ifdocbook", "ifhtml", "ifinfo",
  "ifnotdocbook", "ifnothtml", "ifnotinfo", "ifnotplaintext", "ifnottex",
  "ifnotxml", "ifplaintext", "ifset", "iftex", "ifxml", "itemize", "lisp",
  "menu", "multitable", "quotation", "rawdocbook", "rawhtml", "rawtex",
  "rawxml", "smalldisplay", "smallexample", "smallformat", "smalllisp",
  "verbatim", "table", "tex", "vtable", "titlepage", "bad_type"
};

/* All nested environments.  */
INSERTION_ELT *insertion_stack = NULL;

/* How deeply we're nested.  */
int insertion_level = 0;

/* Set to 1 if we've processed (commentary) text in a @menu that
   wasn't part of a menu item.  */
int had_menu_commentary;

/* How to examine menu lines.  */
int in_detailmenu = 0;

/* Whether to examine menu lines.  */
int in_menu = 0;

/* Set to 1 if <p> is written in normal context. 
   Used for menu and itemize. */
int in_paragraph = 0;

/* Since an insertion is already in the stack before we reach the switch
   statement, we cannot use is_in_insertion_of_type (always returns true.) Also
   making it return the level found, and comparing it with the current level is
   no use, due to the order of stack.  */
static int float_active = 0;

/* Unsetting escape_html blindly causes text inside @html/etc. to be escaped if
   used within a rmacro.  */
static int raw_output_block = 0;

/* Non-zero if a <dl> element has a <dt> element in it.  We use this when
   deciding whether to insert a <br> or not.  */
static int html_deflist_has_term = 0;

void
init_insertion_stack (void)
{
  insertion_stack = NULL;
}

/* Return the type of the current insertion. */
static enum insertion_type
current_insertion_type (void)
{
  return insertion_level ? insertion_stack->insertion : bad_type;
}

/* Return the string which is the function to wrap around items, or NULL
   if we're not in an environment where @item is ok.  */
static char *
current_item_function (void)
{
  int done = 0;
  INSERTION_ELT *elt = insertion_stack;

  /* Skip down through the stack until we find an insertion with an
     itemize function defined, i.e., skip conditionals, @cartouche, etc.  */
  while (!done && elt)
    {
      switch (elt->insertion)
        {
        /* This list should match the one in cm_item.  */
        case ifclear:
        case ifhtml:
        case ifinfo:
        case ifnothtml:
        case ifnotinfo:
        case ifnotplaintext:
        case ifnottex:
	case ifnotxml:
        case ifplaintext:
        case ifset:
        case iftex:
	case ifxml:
        case rawdocbook:
        case rawhtml:
        case rawxml:
        case rawtex:
        case tex:
        case cartouche:
          elt = elt->next;
          break;
      
        default:
          done = 1;
        }
    }

  /* item_function usually gets assigned the empty string.  */
  return done && (*elt->item_function) ? elt->item_function : NULL;
}

/* Parse the item marker function off the input.  If result is just "@",
   change it to "@ ", since "@" by itself is not a command.  This makes
   "@ ", "@\t", and "@\n" all the same, but their default meanings are
   the same anyway, and let's not worry about supporting redefining them.  */
static char *
get_item_function (void)
{
  char *item_function;
  char *item_loc;
  
  get_rest_of_line (0, &item_function);

  /* If the document erroneously says
       @itemize @bullet @item foobar
     it's nicer to give an error up front than repeat `@bullet expected
     braces' until we get a segmentation fault.  */
  item_loc = strstr (item_function, "@item");
  if (item_loc)
    {
      line_error (_("@item not allowed in argument to @itemize"));
      *item_loc = 0;
    }

  /* If we hit the end of text in get_rest_of_line, backing up
     input pointer will cause the last character of the last line
     be pushed back onto the input, which is wrong.  */
  if (input_text_offset < input_text_length)
    backup_input_pointer ();

  if (STREQ (item_function, "@"))
    {
      free (item_function);
      item_function = xstrdup ("@ ");
    }

  return item_function;
}

 /* Push the state of the current insertion on the stack. */
static void
push_insertion (enum insertion_type type, char *item_function)
{
  INSERTION_ELT *new = xmalloc (sizeof (INSERTION_ELT));

  new->item_function = item_function;
  new->filling_enabled = filling_enabled;
  new->indented_fill = indented_fill;
  new->insertion = type;
  new->line_number = line_number;
  new->filename = xstrdup (input_filename);
  new->inhibited = inhibit_paragraph_indentation;
  new->in_fixed_width_font = in_fixed_width_font;
  new->next = insertion_stack;
  insertion_stack = new;
  insertion_level++;
}

 /* Pop the value on top of the insertion stack into the
    global variables. */
void
pop_insertion (void)
{
  INSERTION_ELT *temp = insertion_stack;

  if (temp == NULL)
    return;

  in_fixed_width_font = temp->in_fixed_width_font;
  inhibit_paragraph_indentation = temp->inhibited;
  filling_enabled = temp->filling_enabled;
  indented_fill = temp->indented_fill;
  free_and_clear (&(temp->item_function));
  free_and_clear (&(temp->filename));
  insertion_stack = insertion_stack->next;
  free (temp);
  insertion_level--;
}

 /* Return a pointer to the print name of this
    enumerated type. */
static const char *
insertion_type_pname (enum insertion_type type)
{
  if ((int) type < (int) bad_type)
  {
    if (type == rawdocbook)
      return "docbook";
    else if (type == rawhtml)
      return "html";
    else if (type == rawxml)
      return "xml";
    else if (type == rawtex)
      return "tex";
    else
      return insertion_type_names[(int) type];
  }
  else
    return _("Broken-Type in insertion_type_pname");
}

/* Return the insertion_type associated with NAME.
   If the type is not one of the known ones, return BAD_TYPE. */
enum insertion_type
find_type_from_name (char *name)
{
  int index = 0;
  while (index < (int) bad_type)
    {
      if (STREQ (name, insertion_type_names[index]))
        return (enum insertion_type) index;
      if (index == rawdocbook && STREQ (name, "docbook"))
        return rawdocbook;
      if (index == rawhtml && STREQ (name, "html"))
        return rawhtml;
      if (index == rawxml && STREQ (name, "xml"))
        return rawxml;
      if (index == rawtex && STREQ (name, "tex"))
        return rawtex;
      index++;
    }
  return bad_type;
}

/* Simple function to query insertion_stack to see if we are inside a given
   insertion type. */
int
is_in_insertion_of_type (int type)
{
  INSERTION_ELT *temp = insertion_stack;

  if (!insertion_level)
    return 0;

  while (temp)
    {
      if (temp->insertion == type)
        return 1;
      temp = temp->next;
    }

  return 0;
}


static int
defun_insertion (enum insertion_type type)
{
  return 0
     || (type == defcv)
     || (type == deffn)
     || (type == defivar)
     || (type == defmac)
     || (type == defmethod)
     || (type == defop)
     || (type == defopt)
     || (type == defspec)
     || (type == deftp)
     || (type == deftypecv)
     || (type == deftypefn)
     || (type == deftypefun)
     || (type == deftypeivar)
     || (type == deftypemethod)
     || (type == deftypeop)
     || (type == deftypevar)
     || (type == deftypevr)
     || (type == defun)
     || (type == defvar)
     || (type == defvr)
  ;
}

/* MAX_NS is the maximum nesting level for enumerations.  I picked 100
   which seemed reasonable.  This doesn't control the number of items,
   just the number of nested lists. */
#define max_stack_depth 100
#define ENUM_DIGITS 1
#define ENUM_ALPHA  2
typedef struct {
  int enumtype;
  int enumval;
} DIGIT_ALPHA;

DIGIT_ALPHA enumstack[max_stack_depth];
int enumstack_offset = 0;
int current_enumval = 1;
int current_enumtype = ENUM_DIGITS;
char *enumeration_arg = NULL;

static void
start_enumerating (int at, int type)
{
  if ((enumstack_offset + 1) == max_stack_depth)
    {
      line_error (_("Enumeration stack overflow"));
      return;
    }
  enumstack[enumstack_offset].enumtype = current_enumtype;
  enumstack[enumstack_offset].enumval = current_enumval;
  enumstack_offset++;
  current_enumval = at;
  current_enumtype = type;
}

static void
stop_enumerating (void)
{
  --enumstack_offset;
  if (enumstack_offset < 0)
    enumstack_offset = 0;

  current_enumval = enumstack[enumstack_offset].enumval;
  current_enumtype = enumstack[enumstack_offset].enumtype;
}

/* Place a letter or digits into the output stream. */
static void
enumerate_item (void)
{
  char temp[10];

  if (current_enumtype == ENUM_ALPHA)
    {
      if (current_enumval == ('z' + 1) || current_enumval == ('Z' + 1))
        {
          current_enumval = ((current_enumval - 1) == 'z' ? 'a' : 'A');
          warning (_("lettering overflow, restarting at %c"), current_enumval);
        }
      sprintf (temp, "%c. ", current_enumval);
    }
  else
    sprintf (temp, "%d. ", current_enumval);

  indent (output_column += (current_indent - strlen (temp)));
  add_word (temp);
  current_enumval++;
}

static void
enum_html (void)
{
  char type;
  int start;

  if (isdigit (*enumeration_arg))
    {
      type = '1';
      start = atoi (enumeration_arg);
    }
  else if (isupper (*enumeration_arg))
    {
      type = 'A';
      start = *enumeration_arg - 'A' + 1;
    }
  else
    {
      type = 'a';
      start = *enumeration_arg - 'a' + 1;
    }

  add_html_block_elt_args ("<ol type=%c start=%d>\n", type, start);
}

/* Conditionally parse based on the current command name. */
void
command_name_condition (void)
{
  char *discarder = xmalloc (8 + strlen (command));

  sprintf (discarder, "\n%cend %s", COMMAND_PREFIX, command);
  discard_until (discarder);
  discard_until ("\n");

  free (discarder);
}

/* This is where the work for all the "insertion" style
   commands is done.  A huge switch statement handles the
   various setups, and generic code is on both sides. */
void
begin_insertion (enum insertion_type type)
{
  int no_discard = 0;

  if (defun_insertion (type))
    {
      push_insertion (type, xstrdup (""));
      no_discard++;
    }
  else
    {
      push_insertion (type, get_item_function ());
    }

  switch (type)
    {
    case menu:
      if (!no_headers)
        close_paragraph ();

      filling_enabled = no_indent = 0;
      inhibit_paragraph_indentation = 1;

      if (html)
        {
          had_menu_commentary = 1;
        }
      else if (!no_headers && !xml)
        add_word ("* Menu:\n");

      if (xml)
        xml_insert_element (MENU, START);
      else
        in_fixed_width_font++;

      next_menu_item_number = 1;
      in_menu++;
      no_discard++;
      break;

    case detailmenu:
      if (!in_menu)
        {
          if (!no_headers)
            close_paragraph ();

          filling_enabled = no_indent = 0;
          inhibit_paragraph_indentation = 1;

          no_discard++;
        }

      if (xml)
        {
          xml_insert_element (DETAILMENU, START);
          skip_whitespace_and_newlines();
        }
      else
        in_fixed_width_font++;

      in_detailmenu++;
      break;

    case direntry:
      close_single_paragraph ();
      filling_enabled = no_indent = 0;
      inhibit_paragraph_indentation = 1;
      insert_string ("START-INFO-DIR-ENTRY\n");
      break;

    case documentdescription:
      {
        char *desc;
        int start_of_end;
        int save_fixed_width;

        discard_until ("\n"); /* ignore the @documentdescription line */
        start_of_end = get_until ("\n@end documentdescription", &desc);
        save_fixed_width = in_fixed_width_font;

        in_fixed_width_font = 0;
        document_description = expansion (desc, 0);
        free (desc);

        in_fixed_width_font = save_fixed_width;
        input_text_offset = start_of_end; /* go back to the @end to match */
      }
      break;

    case copying:
        /* Save the copying text away for @insertcopying,
           typically used on the back of the @titlepage (for TeX) and
           the Top node (for info/html).  */
      if (input_text[input_text_offset] != '\n')
        discard_until ("\n"); /* ignore remainder of @copying line */

        input_text_offset = get_until ("\n@end copying", &copying_text);
        canon_white (copying_text);

      /* For info, output the copying text right away, so it will end up
         in the header of the Info file, before the first node, and thus
         get copied automatically to all the split files.  For xml, also
         output it right away since xml output is never split.
         For html, we output it specifically in html_output_head. 
         For plain text, there's no way to hide it, so the author must
          use @insertcopying in the desired location.  */
      if (docbook)
	{
	  if (!xml_in_bookinfo)
	    {
	      xml_insert_element (BOOKINFO, START);
	      xml_in_bookinfo = 1;
	    }
          xml_insert_element (LEGALNOTICE, START);
	}

      if (!html && !no_headers)
        cm_insert_copying ();

      if (docbook)
        xml_insert_element (LEGALNOTICE, END);

      break;

    case quotation:
      /* @quotation does filling (@display doesn't).  */
      if (html)
        add_html_block_elt ("<blockquote>\n");
      else
        {
          /* with close_single_paragraph, we get no blank line above
             within @copying.  */
          close_paragraph ();
          last_char_was_newline = no_indent = 0;
          indented_fill = filling_enabled = 1;
          inhibit_paragraph_indentation = 1;
        }
      current_indent += default_indentation_increment;
      if (xml)
        xml_insert_quotation (insertion_stack->item_function, START);
      else if (strlen(insertion_stack->item_function))
        execute_string ("@b{%s:} ", insertion_stack->item_function);
      break;

    case example:
    case smallexample:
    case lisp:
    case smalllisp:
      in_fixed_width_font++;
      /* fall through */

      /* Like @example but no fixed width font. */
    case display:
    case smalldisplay:
      /* Like @display but without indentation. */
    case smallformat:
    case format:
      close_single_paragraph ();
      inhibit_paragraph_indentation = 1;
      filling_enabled = 0;
      last_char_was_newline = 0;

      if (html)
        /* Kludge alert: if <pre> is followed by a newline, IE3,
           mozilla, maybe others render an extra blank line before the
           pre-formatted block.  So don't output a newline.  */
        add_html_block_elt_args ("<pre class=\"%s\">", command);

      if (type != format && type != smallformat)
        {
          current_indent += example_indentation_increment;
          if (html)
            {
              /* Since we didn't put \n after <pre>, we need to insert
                 the indentation by hand.  */
              int i;
              for (i = current_indent; i > 0; i--)
                add_char (' ');
            }
        }
      break;

    case multitable:
      do_multitable ();
      break;

    case table:
    case ftable:
    case vtable:
    case itemize:
      close_single_paragraph ();
      current_indent += default_indentation_increment;
      filling_enabled = indented_fill = 1;
#if defined (INDENT_PARAGRAPHS_IN_TABLE)
      inhibit_paragraph_indentation = 0;
#else
      inhibit_paragraph_indentation = 1;
#endif /* !INDENT_PARAGRAPHS_IN_TABLE */

      /* Make things work for losers who forget the itemize syntax. */
      if (type == itemize)
        {
          if (!(*insertion_stack->item_function))
            {
              free (insertion_stack->item_function);
              insertion_stack->item_function = xstrdup ("@bullet");
            }
        }

      if (!*insertion_stack->item_function)
        {
          line_error (_("%s requires an argument: the formatter for %citem"),
                      insertion_type_pname (type), COMMAND_PREFIX);
        }

      if (html)
        {
          if (type == itemize)
            {
              add_html_block_elt ("<ul>\n");
              in_paragraph = 0;
            }
          else
            { /* We are just starting, so this <dl>
                 has no <dt> children yet.  */
              html_deflist_has_term = 0;
              add_html_block_elt ("<dl>\n");
            }
        }
      if (xml)
        xml_begin_table (type, insertion_stack->item_function);

      while (input_text[input_text_offset] == '\n'
          && input_text[input_text_offset+1] == '\n')
        {
          line_number++;
          input_text_offset++;
        }

      break;

    case enumerate:
      close_single_paragraph ();
      no_indent = 0;
#if defined (INDENT_PARAGRAPHS_IN_TABLE)
      inhibit_paragraph_indentation = 0;
#else
      inhibit_paragraph_indentation = 1;
#endif /* !INDENT_PARAGRAPHS_IN_TABLE */

      current_indent += default_indentation_increment;
      filling_enabled = indented_fill = 1;

      if (html)
        {
          enum_html ();
          in_paragraph = 0;
        }

      if (xml)
        xml_begin_enumerate (enumeration_arg);
      
      if (isdigit (*enumeration_arg))
        start_enumerating (atoi (enumeration_arg), ENUM_DIGITS);
      else
        start_enumerating (*enumeration_arg, ENUM_ALPHA);
      break;

      /* @group produces no output in info. */
    case group:
      /* Only close the paragraph if we are not inside of an
         @example-like environment. */
      if (xml)
        xml_insert_element (GROUP, START);
      else if (!insertion_stack->next
          || (insertion_stack->next->insertion != display
              && insertion_stack->next->insertion != smalldisplay
              && insertion_stack->next->insertion != example
              && insertion_stack->next->insertion != smallexample
              && insertion_stack->next->insertion != lisp
              && insertion_stack->next->insertion != smalllisp
              && insertion_stack->next->insertion != format
              && insertion_stack->next->insertion != smallformat
              && insertion_stack->next->insertion != flushleft
              && insertion_stack->next->insertion != flushright))
        close_single_paragraph ();
      break;

    case cartouche:
      if (html)
	add_html_block_elt ("<p><table class=\"cartouche\" summary=\"cartouche\" border=\"1\"><tr><td>\n");
      if (in_menu)
        no_discard++;
      break;

    case floatenv:
      /* Cannot nest floats, so complain.  */
      if (float_active)
        {
          line_error (_("%cfloat environments cannot be nested"), COMMAND_PREFIX);
          pop_insertion ();
          break;
        }

      float_active++;

      { /* Collect data about this float.  */
        /* Example: @float [FLOATTYPE][,XREFLABEL][,POSITION] */
        char floattype[200] = "";
        char xreflabel[200] = "";
        char position[200]  = "";
        char *text;
        char *caption;
        char *shortcaption;
        int start_of_end;
        int save_line_number = line_number;
        int save_input_text_offset = input_text_offset;
        int i;

        if (strlen (insertion_stack->item_function) > 0)
          {
            int i = 0, t = 0, c = 0;
            while (insertion_stack->item_function[i])
              {
                if (insertion_stack->item_function[i] == ',')
                  {
                    switch (t)
                      {
                      case 0:
                        floattype[c] = '\0';
                        break;
                      case 1:
                        xreflabel[c] = '\0';
                        break;
                      case 2:
                        position[c] = '\0';
                        break;
                      }
                    c = 0;
                    t++;
                    i++;
                    continue;
                  }

                switch (t)
                  {
                  case 0:
                    floattype[c] = insertion_stack->item_function[i];
                    break;
                  case 1:
                    xreflabel[c] = insertion_stack->item_function[i];
                    break;
                  case 2:
                    position[c] = insertion_stack->item_function[i];
                    break;
                  }
                c++;
                i++;
              }
          }

        skip_whitespace_and_newlines ();

        start_of_end = get_until ("\n@end float", &text);

        /* Get also the @caption.  */
        i = search_forward_until_pos ("\n@caption{",
            save_input_text_offset, start_of_end);
        if (i > -1)
          {
            input_text_offset = i + sizeof ("\n@caption{") - 1;
            get_until_in_braces ("\n@end float", &caption);
            input_text_offset = save_input_text_offset;
          }
        else
          caption = "";

        /* ... and the @shortcaption.  */
        i = search_forward_until_pos ("\n@shortcaption{",
            save_input_text_offset, start_of_end);
        if (i > -1)
          {
            input_text_offset = i + sizeof ("\n@shortcaption{") - 1;
            get_until_in_braces ("\n@end float", &shortcaption);
            input_text_offset = save_input_text_offset;
          }
        else
          shortcaption = "";

        canon_white (xreflabel);
        canon_white (floattype);
        canon_white (position);
        canon_white (caption);
        canon_white (shortcaption);

        add_new_float (xstrdup (xreflabel),
            xstrdup (caption), xstrdup (shortcaption),
            xstrdup (floattype), xstrdup (position));

        /* Move to the start of the @float so the contents get processed as
           usual.  */
        input_text_offset = save_input_text_offset;
        line_number = save_line_number;
      }

      if (html)
        add_html_block_elt ("<div class=\"float\">\n");
      else if (docbook)
        xml_insert_element (FLOAT, START);
      else if (xml)
        {
          xml_insert_element_with_attribute (FLOAT, START,
              "name=\"%s\"", current_float_id ());

          xml_insert_element (FLOATTYPE, START);
          execute_string ("%s", current_float_type ());
          xml_insert_element (FLOATTYPE, END);

          xml_insert_element (FLOATPOS, START);
          execute_string ("%s", current_float_position ());
          xml_insert_element (FLOATPOS, END);
        }
      else
        { /* Info */
          close_single_paragraph ();
          inhibit_paragraph_indentation = 1;
        }

      /* Anchor now.  Note that XML documents get their
         anchors with <float name="anchor"> tag.  */
      if ((!xml || docbook) && strlen (current_float_id ()) > 0)
        execute_string ("@anchor{%s}", current_float_id ());

      break;

      /* Insertions that are no-ops in info, but do something in TeX. */
    case ifclear:
    case ifdocbook:
    case ifhtml:
    case ifinfo:
    case ifnotdocbook:
    case ifnothtml:
    case ifnotinfo:
    case ifnotplaintext:
    case ifnottex:
    case ifnotxml:
    case ifplaintext:
    case ifset:
    case iftex:
    case ifxml:
    case rawtex:
      if (in_menu)
        no_discard++;
      break;

    case rawdocbook:
    case rawhtml:
    case rawxml:
      raw_output_block++;

      if (raw_output_block > 0)
        {
          xml_no_para = 1;
          escape_html = 0;
          xml_keep_space++;
        }

      {
        /* Some deuglification for improved readability.  */
        extern int xml_in_para;
        if (xml && !xml_in_para && xml_indentation_increment > 0)
          add_char ('\n');
      }

      break;

    case defcv:
    case deffn:
    case defivar:
    case defmac:
    case defmethod:
    case defop:
    case defopt:
    case defspec:
    case deftp:
    case deftypecv:
    case deftypefn:
    case deftypefun:
    case deftypeivar:
    case deftypemethod:
    case deftypeop:
    case deftypevar:
    case deftypevr:
    case defun:
    case defvar:
    case defvr:
      inhibit_paragraph_indentation = 1;
      filling_enabled = indented_fill = 1;
      current_indent += default_indentation_increment;
      no_indent = 0;
      if (xml)
	xml_begin_definition ();
      break;

    case flushleft:
      close_single_paragraph ();
      inhibit_paragraph_indentation = 1;
      filling_enabled = indented_fill = no_indent = 0;
      if (html)
        add_html_block_elt ("<div align=\"left\">");
      break;

    case flushright:
      close_single_paragraph ();
      filling_enabled = indented_fill = no_indent = 0;
      inhibit_paragraph_indentation = 1;
      force_flush_right++;
      if (html)
        add_html_block_elt ("<div align=\"right\">");
      break;

    case titlepage:
      xml_insert_element (TITLEPAGE, START);
      break;

    default:
      line_error ("begin_insertion internal error: type=%d", type);
    }

  if (!no_discard)
    discard_until ("\n");
}

/* Try to end the insertion with the specified TYPE.  With a value of
   `bad_type', TYPE gets translated to match the value currently on top
   of the stack.  Otherwise, if TYPE doesn't match the top of the
   insertion stack, give error. */
static void
end_insertion (int type)
{
  int temp_type;

  if (!insertion_level)
    return;

  temp_type = current_insertion_type ();

  if (type == bad_type)
    type = temp_type;

  if (type != temp_type)
    {
      line_error
        (_("`@end' expected `%s', but saw `%s'"),
         insertion_type_pname (temp_type), insertion_type_pname (type));
      return;
    }

  pop_insertion ();

  if (xml)
    {
      switch (type)
        {
        case ifinfo:
        case documentdescription:       
          break;
        case quotation:
          xml_insert_quotation ("", END);
          break;
        case example:
          xml_insert_element (EXAMPLE, END);
          if (docbook && current_insertion_type () == floatenv)
            xml_insert_element (FLOATEXAMPLE, END);
          break;
        case smallexample:
          xml_insert_element (SMALLEXAMPLE, END);
          if (docbook && current_insertion_type () == floatenv)
            xml_insert_element (FLOATEXAMPLE, END);
          break;
        case lisp:
          xml_insert_element (LISP, END);
          if (docbook && current_insertion_type () == floatenv)
            xml_insert_element (FLOATEXAMPLE, END);
          break;
        case smalllisp:
          xml_insert_element (SMALLLISP, END);
          if (docbook && current_insertion_type () == floatenv)
            xml_insert_element (FLOATEXAMPLE, END);
          break;
        case cartouche:
          xml_insert_element (CARTOUCHE, END);
          break;
        case format:
	  if (docbook && xml_in_bookinfo && xml_in_abstract)
	    {
	      xml_insert_element (ABSTRACT, END);
	      xml_in_abstract = 0;
	    }
	  else
	    xml_insert_element (FORMAT, END);
          break;
        case smallformat:
          xml_insert_element (SMALLFORMAT, END);
          break;
        case display:
          xml_insert_element (DISPLAY, END);
          break;
        case smalldisplay:
          xml_insert_element (SMALLDISPLAY, END);
          break;
        case table:
        case ftable:
        case vtable:      
        case itemize:
          xml_end_table (type);
          break;
        case enumerate:
          xml_end_enumerate ();
          break;
        case group:
          xml_insert_element (GROUP, END);
          break;
	case titlepage:
	  xml_insert_element (TITLEPAGE, END);
	  break;
        }
    }
  switch (type)
    {
      /* Insertions which have no effect on paragraph formatting. */
    case copying:
      line_number--;
      break;

    case ifclear:
    case ifdocbook:
    case ifinfo:
    case ifhtml:
    case ifnotdocbook:
    case ifnothtml:
    case ifnotinfo:
    case ifnotplaintext:
    case ifnottex:
    case ifnotxml:
    case ifplaintext:
    case ifset:
    case iftex:
    case ifxml:
    case rawtex:
    case titlepage:
      break;

    case rawdocbook:
    case rawhtml:
    case rawxml:
      raw_output_block--;

      if (raw_output_block <= 0)
        {
          xml_no_para = 0;
          escape_html = 1;
          xml_keep_space--;
        }

      if ((xml || html) && output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;
      break;

    case detailmenu:
      if (xml)
        xml_insert_element (DETAILMENU, END);

      in_detailmenu--;          /* No longer hacking menus. */
      if (!in_menu)
        {
          if (!no_headers)
            close_insertion_paragraph ();
        }
      break;

    case direntry:              /* Eaten if html. */
      insert_string ("END-INFO-DIR-ENTRY\n\n");
      close_insertion_paragraph ();
      break;

    case documentdescription:
      if (xml)
        insert_string (document_description);
        xml_insert_element (DOCUMENTDESCRIPTION, END);
      break;
      
    case menu:
      in_menu--;                /* No longer hacking menus. */
      if (html && !no_headers)
        add_html_block_elt ("</ul>\n");
      else if (!no_headers && !xml)
        close_insertion_paragraph ();
      break;

    case multitable:
      end_multitable ();
      break;

    case enumerate:
      stop_enumerating ();
      close_insertion_paragraph ();
      current_indent -= default_indentation_increment;
      if (html)
        add_html_block_elt ("</ol>\n");
      break;

    case flushleft:
      if (html)
        add_html_block_elt ("</div>\n");
      close_insertion_paragraph ();
      break;

    case cartouche:
      if (html)
	add_html_block_elt ("</td></tr></table>\n");
      close_insertion_paragraph ();
      break;

    case group:
      if (!xml || docbook)
        close_insertion_paragraph ();
      break;

    case floatenv:
      if (xml)
        xml_insert_element (FLOAT, END);
      else
        {
          if (html)
            add_html_block_elt ("<p><strong class=\"float-caption\">");
          else
            close_paragraph ();

          no_indent = 1;

          /* Legend:
               1) @float Foo,lbl & no caption:    Foo 1.1
               2) @float Foo & no caption:        Foo
               3) @float ,lbl & no caption:       1.1
               4) @float & no caption:                    */

          if (!xml && !html)
            indent (current_indent);

          if (strlen (current_float_type ()))
            execute_string ("%s", current_float_type ());

          if (strlen (current_float_id ()) > 0)
            {
              if (strlen (current_float_type ()) > 0)
                add_char (' ');

              add_word (current_float_number ());
            }

          if (strlen (current_float_title ()) > 0)
            {
              if (strlen (current_float_type ()) > 0
                  || strlen (current_float_id ()) > 0)
                insert_string (": ");

              execute_string ("%s", current_float_title ());
            }

          /* Indent the following paragraph. */
          inhibit_paragraph_indentation = 0;

          if (html)
            add_word ("</strong></p></div>\n");
          else
            close_paragraph ();
        }
      float_active--;
      break;

    case format:
    case smallformat:
    case display:
    case smalldisplay:
    case example:
    case smallexample:
    case lisp:
    case smalllisp:
    case quotation:
      /* @format and @smallformat are the only fixed_width insertion
         without a change in indentation. */
      if (type != format && type != smallformat && type != quotation)
        current_indent -= example_indentation_increment;
      else if (type == quotation)
        current_indent -= default_indentation_increment;

      if (html)
        { /* The complex code in close_paragraph that kills whitespace
             does not function here, since we've inserted non-whitespace
             (the </whatever>) before it.  The indentation already got
             inserted at the end of the last example line, so we have to
             delete it, or browsers wind up showing an extra blank line.  */
          kill_self_indent (default_indentation_increment);
          add_html_block_elt (type == quotation
              ? "</blockquote>\n" : "</pre>\n");
        }

      /* The ending of one of these insertions always marks the
         start of a new paragraph, except for the XML output. */
      if (!xml || docbook)
        close_insertion_paragraph ();

      /* </pre> closes paragraph without messing with </p>.  */
      if (html && type != quotation)
          paragraph_is_open = 0;
      break;

    case table:
    case ftable:
    case vtable:
      current_indent -= default_indentation_increment;
      if (html)
        add_html_block_elt ("</dl>\n");
      close_insertion_paragraph ();
      break;

    case itemize:
      current_indent -= default_indentation_increment;
      if (html)
        add_html_block_elt ("</ul>\n");
      close_insertion_paragraph ();
      break;

    case flushright:
      force_flush_right--;
      if (html)
        add_html_block_elt ("</div>\n");
      close_insertion_paragraph ();
      break;

    /* Handle the @defun insertions with this default clause. */
    default:
      {
        int base_type;

        if (type < defcv || type > defvr)
          line_error ("end_insertion internal error: type=%d", type);
  
        base_type = get_base_type (type);
        switch (base_type)
          {
          case deffn:
          case defvr:
          case deftp:
          case deftypecv:
          case deftypefn:
          case deftypevr:
          case defcv:
          case defop:
          case deftypemethod:
          case deftypeop:
          case deftypeivar:
            if (html)
              {
                if (paragraph_is_open)
                  add_html_block_elt ("</p>");
                /* close the div and blockquote which has been opened in defun.c */
                if (!rollback_empty_tag ("blockquote"))
                  add_html_block_elt ("</blockquote>");
                add_html_block_elt ("</div>\n");
              }
	    if (xml)
	      xml_end_definition ();
            break;
          } /* switch (base_type)... */
  
        current_indent -= default_indentation_increment;
        close_insertion_paragraph ();
      }
      break;
      
    }

  if (current_indent < 0)
    line_error ("end_insertion internal error: current indent=%d",
                current_indent);
}

/* Insertions cannot cross certain boundaries, such as node beginnings.  In
   code that creates such boundaries, you should call `discard_insertions'
   before doing anything else.  It prints the errors for you, and cleans up
   the insertion stack.

   With nonzero SPECIALS_OK argument, allows unmatched
   @if... conditionals, otherwise not.  This is because conditionals can
   cross node boundaries.  Always happens with the @top node, for example.  */
void
discard_insertions (int specials_ok)
{
  int real_line_number = line_number;
  while (insertion_stack)
    {
      if (specials_ok
          && ((ifclear <= insertion_stack->insertion
               && insertion_stack->insertion <= iftex)
              || insertion_stack->insertion == rawdocbook
              || insertion_stack->insertion == rawhtml
              || insertion_stack->insertion == rawxml
              || insertion_stack->insertion == rawtex))
        break;
      else
        {
          const char *offender = insertion_type_pname (insertion_stack->insertion);

          file_line_error (insertion_stack->filename,
                           insertion_stack->line_number,
                           _("No matching `%cend %s'"), COMMAND_PREFIX,
                           offender);
          pop_insertion ();
        }
    }
  line_number = real_line_number;
}

/* Insertion (environment) commands.  */

void
cm_quotation (void)
{
  /* We start the blockquote element in the insertion.  */
  begin_insertion (quotation);
}

void
cm_example (void)
{
  if (docbook && current_insertion_type () == floatenv)
    xml_begin_docbook_float (FLOATEXAMPLE);

  if (xml)
    {
      /* Rollback previous newlines.  These occur between
         </para> and <example>.  */
      if (output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;

      xml_insert_element (EXAMPLE, START);

      /* Make sure example text is starting on a new line
         for improved readability.  */
      if (docbook)
        add_char ('\n');
    }

  begin_insertion (example);
}

void
cm_smallexample (void)
{
  if (docbook && current_insertion_type () == floatenv)
    xml_begin_docbook_float (FLOATEXAMPLE);

  if (xml)
    {
      /* See cm_example comments about newlines.  */
      if (output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;
      xml_insert_element (SMALLEXAMPLE, START);
      if (docbook)
        add_char ('\n');
    }

  begin_insertion (smallexample);
}

void
cm_lisp (void)
{
  if (docbook && current_insertion_type () == floatenv)
    xml_begin_docbook_float (FLOATEXAMPLE);

  if (xml)
    {
      /* See cm_example comments about newlines.  */
      if (output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;
      xml_insert_element (LISP, START);
      if (docbook)
        add_char ('\n');
    }

  begin_insertion (lisp);
}

void
cm_smalllisp (void)
{
  if (docbook && current_insertion_type () == floatenv)
    xml_begin_docbook_float (FLOATEXAMPLE);

  if (xml)
    {
      /* See cm_example comments about newlines.  */
      if (output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;
      xml_insert_element (SMALLLISP, START);
      if (docbook)
        add_char ('\n');
    }

  begin_insertion (smalllisp);
}

void
cm_cartouche (void)
{
  if (docbook && current_insertion_type () == floatenv)
    xml_begin_docbook_float (CARTOUCHE);

  if (xml)
    xml_insert_element (CARTOUCHE, START);
  begin_insertion (cartouche);
}

void
cm_copying (void)
{
  begin_insertion (copying);
}

/* Not an insertion, despite the name, but it goes with cm_copying.  */
void
cm_insert_copying (void)
{
  if (!copying_text)
    {
      warning ("@copying not used before %s", command);
      return;
    }

  execute_string ("%s", copying_text);

  if (!xml && !html)
    {
      add_word ("\n\n");
      /* Update output_position so that the node positions in the tag
         tables will take account of the copying text.  */
      flush_output ();
    }
}

void
cm_format (void)
{
  if (xml)
    {
      if (docbook && xml_in_bookinfo)
	{
	  xml_insert_element (ABSTRACT, START);
	  xml_in_abstract = 1;
	}
      else
        {
          /* See cm_example comments about newlines.  */
          if (output_paragraph[output_paragraph_offset-1] == '\n')
            output_paragraph_offset--;
          xml_insert_element (FORMAT, START);
          if (docbook)
            add_char ('\n');
        }
    }
  begin_insertion (format);
}

void
cm_smallformat (void)
{
  if (xml)
    {
      /* See cm_example comments about newlines.  */
      if (output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;
      xml_insert_element (SMALLFORMAT, START);
      if (docbook)
        add_char ('\n');
    }

  begin_insertion (smallformat);
}

void
cm_display (void)
{
  if (xml)
    {
      /* See cm_example comments about newlines.  */
      if (output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;
      xml_insert_element (DISPLAY, START);
      if (docbook)
        add_char ('\n');
    }

  begin_insertion (display);
}

void
cm_smalldisplay (void)
{
  if (xml)
    {
      /* See cm_example comments about newlines.  */
      if (output_paragraph[output_paragraph_offset-1] == '\n')
        output_paragraph_offset--;
      xml_insert_element (SMALLDISPLAY, START);
      if (docbook)
        add_char ('\n');
    }

  begin_insertion (smalldisplay);
}

void
cm_direntry (void)
{
  if (html || xml || no_headers)
    command_name_condition ();
  else
    begin_insertion (direntry);
}

void
cm_documentdescription (void)
{
  if (html)
    begin_insertion (documentdescription);

  else if (xml)
    {
      xml_insert_element (DOCUMENTDESCRIPTION, START);
      begin_insertion (documentdescription);
    }

  else
    command_name_condition ();
}


void
cm_itemize (void)
{
  begin_insertion (itemize);
}

/* Start an enumeration insertion of type TYPE.  If the user supplied
   no argument on the line, then use DEFAULT_STRING as the initial string. */
static void
do_enumeration (int type, char *default_string)
{
  get_until_in_line (0, ".", &enumeration_arg);
  canon_white (enumeration_arg);

  if (!*enumeration_arg)
    {
      free (enumeration_arg);
      enumeration_arg = xstrdup (default_string);
    }

  if (!isdigit (*enumeration_arg) && !isletter (*enumeration_arg))
    {
      warning (_("%s requires letter or digit"), insertion_type_pname (type));

      switch (type)
        {
        case enumerate:
          default_string = "1";
          break;
        }
      enumeration_arg = xstrdup (default_string);
    }
  begin_insertion (type);
}

void
cm_enumerate (void)
{
  do_enumeration (enumerate, "1");
}

/*  Handle verbatim environment:
    find_end_verbatim == 0:  process until end of file
    find_end_verbatim != 0:  process until 'COMMAND_PREFIXend verbatim'
                             or end of file

  We cannot simply copy input stream onto output stream; as the
  verbatim environment may be encapsulated in an @example environment,
  for example. */
void
handle_verbatim_environment (int find_end_verbatim)
{
  int character;
  int seen_end = 0;
  int save_filling_enabled = filling_enabled;
  int save_inhibit_paragraph_indentation = inhibit_paragraph_indentation;
  int save_escape_html = escape_html;

  if (!insertion_stack)
    close_single_paragraph (); /* no blank lines if not at outer level */
  inhibit_paragraph_indentation = 1;
  filling_enabled = 0;
  in_fixed_width_font++;
  last_char_was_newline = 0;

  /* No indentation: this is verbatim after all
     If you want indent, enclose @verbatim in @example
       current_indent += default_indentation_increment;
   */

  if (html)
    { /* If inside @example, we'll be preceded by the indentation
         already.  Browsers will ignore those spaces because we're about
         to start another <pre> (don't ask me).  So, wipe them out for
         cleanliness, and re-insert.  */
      int i;
      kill_self_indent (default_indentation_increment);
      add_html_block_elt ("<pre class=\"verbatim\">");
      for (i = current_indent; i > 0; i--)
        add_char (' ');
    }
  else if (xml)
    {
      xml_insert_element (VERBATIM, START);
      escape_html = 0;
      add_word ("<![CDATA[");
    }

  while (input_text_offset < input_text_length)
    {
      character = curchar ();

      if (character == '\n')
        line_number++;

      /* Assume no newlines in END_VERBATIM. */
      else if (find_end_verbatim && (character == COMMAND_PREFIX) /* @ */
          && (input_text_length - input_text_offset > sizeof (END_VERBATIM))
          && !strncmp (&input_text[input_text_offset+1], END_VERBATIM,
                       sizeof (END_VERBATIM)-1))
        {
          input_text_offset += sizeof (END_VERBATIM);
          seen_end = 1;
          break;
        }

      if (html && character == '&' && escape_html)
        add_word ("&amp;");
      else if (html && character == '<' && escape_html)
        add_word ("&lt;");
      else
        add_char (character);

      input_text_offset++;
    }

  if (find_end_verbatim && !seen_end)
    warning (_("end of file inside verbatim block"));

  if (html)
    { /* See comments in example case above.  */
      kill_self_indent (default_indentation_increment);
      add_word ("</pre>");
    }
  else if (xml)
    {
      add_word ("]]>");
      xml_insert_element (VERBATIM, END);
      escape_html = save_escape_html;
    }
  
  in_fixed_width_font--;
  filling_enabled = save_filling_enabled;
  inhibit_paragraph_indentation = save_inhibit_paragraph_indentation;
}

void
cm_verbatim (void)
{
  handle_verbatim_environment (1);
}

void
cm_table (void)
{
  begin_insertion (table);
}

void
cm_multitable (void)
{
  begin_insertion (multitable); /* @@ */
}

void
cm_ftable (void)
{
  begin_insertion (ftable);
}

void
cm_vtable (void)
{
  begin_insertion (vtable);
}

void
cm_group (void)
{
  begin_insertion (group);
}

/* Insert raw HTML (no escaping of `<' etc.). */
void
cm_html (int arg)
{
  if (process_html)
    begin_insertion (rawhtml);
  else
    command_name_condition ();
}

void
cm_xml (int arg)
{
  if (process_xml)
    begin_insertion (rawxml);
  else
    command_name_condition ();
}

void
cm_docbook (int arg)
{
  if (process_docbook)
    begin_insertion (rawdocbook);
  else
    command_name_condition ();
}

void
cm_ifdocbook (void)
{
  if (process_docbook)
    begin_insertion (ifdocbook);
  else
    command_name_condition ();
}

void
cm_ifnotdocbook (void)
{
  if (!process_docbook)
    begin_insertion (ifnotdocbook);
  else
    command_name_condition ();
}

void
cm_ifhtml (void)
{
  if (process_html)
    begin_insertion (ifhtml);
  else
    command_name_condition ();
}

void
cm_ifnothtml (void)
{
  if (!process_html)
    begin_insertion (ifnothtml);
  else
    command_name_condition ();
}


void
cm_ifinfo (void)
{
  if (process_info)
    begin_insertion (ifinfo);
  else
    command_name_condition ();
}

void
cm_ifnotinfo (void)
{
  if (!process_info)
    begin_insertion (ifnotinfo);
  else
    command_name_condition ();
}


void
cm_ifplaintext (void)
{
  if (process_plaintext)
    begin_insertion (ifplaintext);
  else
    command_name_condition ();
}

void
cm_ifnotplaintext (void)
{
  if (!process_plaintext)
    begin_insertion (ifnotplaintext);
  else
    command_name_condition ();
}


void
cm_tex (void)
{
  if (process_tex)
    begin_insertion (rawtex);
  else
    command_name_condition ();
}

void
cm_iftex (void)
{
  if (process_tex)
    begin_insertion (iftex);
  else
    command_name_condition ();
}

void
cm_ifnottex (void)
{
  if (!process_tex)
    begin_insertion (ifnottex);
  else
    command_name_condition ();
}

void
cm_ifxml (void)
{
  if (process_xml)
    begin_insertion (ifxml);
  else
    command_name_condition ();
}

void
cm_ifnotxml (void)
{
  if (!process_xml)
    begin_insertion (ifnotxml);
  else
    command_name_condition ();
}


/* Generic xrefable block with a caption.  */
void
cm_float (void)
{
  begin_insertion (floatenv);
}

void
cm_caption (int arg)
{
  char *temp;

  /* This is a no_op command for most formats, as we handle it during @float
     insertion.  For XML though, we handle it here to keep document structure
     as close as possible, to the Texinfo source.  */

  /* Everything is already handled at START.  */
  if (arg == END)
    return;

  /* Check if it's mislocated.  */
  if (current_insertion_type () != floatenv)
    line_error (_("@%s not meaningful outside `@float' environment"), command);

  get_until_in_braces ("\n@end float", &temp);

  if (xml)
    {
      int elt = STREQ (command, "shortcaption") ? SHORTCAPTION : CAPTION;
      xml_insert_element (elt, START);
      if (!docbook)
        execute_string ("%s", temp);
      xml_insert_element (elt, END);
    }

  free (temp);
}

/* Begin an insertion where the lines are not filled or indented. */
void
cm_flushleft (void)
{
  begin_insertion (flushleft);
}

/* Begin an insertion where the lines are not filled, and each line is
   forced to the right-hand side of the page. */
void
cm_flushright (void)
{
  begin_insertion (flushright);
}

void
cm_menu (void)
{
  if (current_node == NULL && !macro_expansion_output_stream)
    {
      warning (_("@menu seen before first @node, creating `Top' node"));
      warning (_("perhaps your @top node should be wrapped in @ifnottex rather than @ifinfo?"));
      /* Include @top command so we can construct the implicit node tree.  */
      execute_string ("@node top\n@top Top\n");
    }
  begin_insertion (menu);
}

void
cm_detailmenu (void)
{
  if (current_node == NULL && !macro_expansion_output_stream)
    { /* Problems anyway, @detailmenu should always be inside @menu.  */
      warning (_("@detailmenu seen before first node, creating `Top' node"));
      execute_string ("@node top\n@top Top\n");
    }
  begin_insertion (detailmenu);
}

/* Title page commands. */

void
cm_titlepage (void)
{
  titlepage_cmd_present = 1;
  if (xml && !docbook)
    begin_insertion (titlepage);
  else
    command_name_condition ();
}

void
cm_author (void)
{
  char *rest;
  get_rest_of_line (1, &rest);

  if (is_in_insertion_of_type (quotation))
    {
      if (html)
        add_word_args ("&mdash; %s", rest);
      else if (docbook)
        {
          /* FIXME Ideally, we should use an attribution element,
             but they are supposed to be at the start of quotation
             blocks.  So to avoid looking ahead mess, let's just
             use mdash like HTML for now.  */
          xml_insert_entity ("mdash");
          add_word (rest);
        }
      else if (xml)
        {
          xml_insert_element (AUTHOR, START);
          add_word (rest);
          xml_insert_element (AUTHOR, END);
        }
      else
        add_word_args ("-- %s", rest);
    }
  else if (is_in_insertion_of_type (titlepage))
    {
      if (xml && !docbook)
        {
          xml_insert_element (AUTHOR, START);
          add_word (rest);
          xml_insert_element (AUTHOR, END);
        }
    }
  else
    line_error (_("@%s not meaningful outside `@titlepage' and `@quotation' environments"),
        command);

  free (rest);
}

void
cm_titlepage_cmds (void)
{
  char *rest;

  get_rest_of_line (1, &rest);

  if (!is_in_insertion_of_type (titlepage))
    line_error (_("@%s not meaningful outside `@titlepage' environment"),
        command);

  if (xml && !docbook)
    {
      int elt = 0;

      if (STREQ (command, "title"))
        elt = BOOKTITLE;
      else if (STREQ (command, "subtitle"))
        elt = BOOKSUBTITLE;

      xml_insert_element (elt, START);
      add_word (rest);
      xml_insert_element (elt, END);
    }

    free (rest);
}

/* End existing insertion block. */
void
cm_end (void)
{
  char *temp;
  int type;

  get_rest_of_line (0, &temp);

  if (!insertion_level)
    {
      line_error (_("Unmatched `%c%s'"), COMMAND_PREFIX, command);
      return;
    }

  if (temp[0] == 0)
    line_error (_("`%c%s' needs something after it"), COMMAND_PREFIX, command);

  type = find_type_from_name (temp);

  if (type == bad_type)
    {
      line_error (_("Bad argument `%s' to `@%s', using `%s'"),
           temp, command, insertion_type_pname (current_insertion_type ()));
    }
  if (xml && type == menu) /* fixme */
    {
      xml_end_menu ();
    }
  end_insertion (type);
  free (temp);
}

/* @itemx, @item. */

static int itemx_flag = 0;

/* Return whether CMD takes a brace-delimited {arg}.  */
int
command_needs_braces (char *cmd)
{
  int i;
  for (i = 0; command_table[i].name; i++)
    {
      if (STREQ (command_table[i].name, cmd))
        return command_table[i].argument_in_braces == BRACE_ARGS;
    }

  return 0; /* macro or alias */
}


void
cm_item (void)
{
  char *rest_of_line, *item_func;

  /* Can only hack "@item" while inside of an insertion. */
  if (insertion_level)
    {
      INSERTION_ELT *stack = insertion_stack;
      int original_input_text_offset;

      skip_whitespace ();
      original_input_text_offset = input_text_offset;

      get_rest_of_line (0, &rest_of_line);
      item_func = current_item_function ();

    /* Do the right thing depending on which insertion function is active. */
    switch_top:
      switch (stack->insertion)
        {
        case multitable:
          multitable_item ();
          /* Support text directly after the @item.  */
          if (*rest_of_line)
            {
              line_number--;
              input_text_offset = original_input_text_offset;
            }
          break;

        case ifclear:
        case ifhtml:
        case ifinfo:
        case ifnothtml:
        case ifnotinfo:
        case ifnotplaintext:
        case ifnottex:
	case ifnotxml:
        case ifplaintext:
        case ifset:
        case iftex:
	case ifxml:
        case rawdocbook:
        case rawhtml:
        case rawxml:
        case rawtex:
        case tex:
        case cartouche:
          stack = stack->next;
          if (!stack)
            goto no_insertion;
          else
            goto switch_top;
          break;

        case menu:
        case quotation:
        case example:
        case smallexample:
        case lisp:
        case smalllisp:
        case format:
        case smallformat:
        case display:
        case smalldisplay:
        case group:
          line_error (_("@%s not meaningful inside `@%s' block"),
                      command,
                      insertion_type_pname (current_insertion_type ()));
          break;

        case itemize:
        case enumerate:
          if (itemx_flag)
            {
              line_error (_("@itemx not meaningful inside `%s' block"),
                          insertion_type_pname (current_insertion_type ()));
            }
          else
            {
              if (html)
                add_html_block_elt ("<li>");
              else if (xml)
                xml_begin_item ();
              else
                {
                  start_paragraph ();
                  kill_self_indent (-1);
                  filling_enabled = indented_fill = 1;

                  if (current_item_function ())
                    {
                      output_column = current_indent - 2;
                      indent (output_column);

                      /* The item marker can be given with or without
                         braces -- @bullet and @bullet{} are both ok.
                         Or it might be something that doesn't take
                         braces at all, such as "o" or "#" or "@ ".
                         Thus, only supply braces if the item marker is
                         a command, they haven't supplied braces
                         themselves, and we know it needs them.  */
                      if (item_func && *item_func)
                        {
                          if (*item_func == COMMAND_PREFIX
                              && item_func[strlen (item_func) - 1] != '}'
                              && command_needs_braces (item_func + 1))
                            execute_string ("%s{}", item_func);
                          else
                            execute_string ("%s", item_func);
                        }
                      insert (' ');
                      output_column++;
                    }
                  else
                    enumerate_item ();

                  /* Special hack.  This makes `close_paragraph' a no-op until
                     `start_paragraph' has been called. */
                  must_start_paragraph = 1;
                }

              /* Handle text directly after the @item.  */
              if (*rest_of_line)
                {
                  line_number--;
                  input_text_offset = original_input_text_offset;
                }
            }
          break;

        case table:
        case ftable:
        case vtable:
          if (html)
            { /* If nothing has been output since the last <dd>,
                 remove the empty <dd> element.  Some browsers render
                 an extra empty line for <dd><dt>, which makes @itemx
                 conversion look ugly.  */
              rollback_empty_tag ("dd");

              /* Force the browser to render one blank line before
                 each new @item in a table.  But don't do that if
                 this is the first <dt> after the <dl>, or if we are
                 converting @itemx.

                 Note that there are some browsers which ignore <br>
                 in this context, but I cannot find any way to force
                 them all render exactly one blank line.  */
              if (!itemx_flag && html_deflist_has_term)
                add_html_block_elt ("<br>");

              /* We are about to insert a <dt>, so this <dl> has a term.
                 Feel free to insert a <br> next time. :)  */
              html_deflist_has_term = 1;
   
              add_html_block_elt ("<dt>");
              if (item_func && *item_func)
                execute_string ("%s{%s}", item_func, rest_of_line);
              else
                execute_string ("%s", rest_of_line);

              if (current_insertion_type () == ftable)
                execute_string ("%cfindex %s\n", COMMAND_PREFIX, rest_of_line);

              if (current_insertion_type () == vtable)
                execute_string ("%cvindex %s\n", COMMAND_PREFIX, rest_of_line);

              add_html_block_elt ("<dd>");
            }
          else if (xml) /* && docbook)*/ /* 05-08 */
            {
              xml_begin_table_item ();

              if (!docbook && current_insertion_type () == ftable)
                execute_string ("%cfindex %s\n", COMMAND_PREFIX, rest_of_line);

              if (!docbook && current_insertion_type () == vtable)
                execute_string ("%cvindex %s\n", COMMAND_PREFIX, rest_of_line);

              if (item_func && *item_func)
                execute_string ("%s{%s}", item_func, rest_of_line);
              else
                execute_string ("%s", rest_of_line);
              xml_continue_table_item ();
            }
          else
            {
              /* We need this to determine if we have two @item's in a row
                 (see test just below).  */
              static int last_item_output_position = 0;

              /* Get rid of extra characters. */
              kill_self_indent (-1);

              /* If we have one @item followed directly by another @item,
                 we need to insert a blank line.  This is not true for
                 @itemx, though.  */
              if (!itemx_flag && last_item_output_position == output_position)
                insert ('\n');

              /* `close_paragraph' almost does what we want.  The problem
                 is when paragraph_is_open, and last_char_was_newline, and
                 the last newline has been turned into a space, because
                 filling_enabled. I handle it here. */
              if (last_char_was_newline && filling_enabled &&
                  paragraph_is_open)
                insert ('\n');
              close_paragraph ();

#if defined (INDENT_PARAGRAPHS_IN_TABLE)
              /* Indent on a new line, but back up one indentation level. */
              {
                int save = inhibit_paragraph_indentation;
                inhibit_paragraph_indentation = 1;
                /* At this point, inserting any non-whitespace character will
                   force the existing indentation to be output. */
                add_char ('i');
                inhibit_paragraph_indentation = save;
              }
#else /* !INDENT_PARAGRAPHS_IN_TABLE */
              add_char ('i');
#endif /* !INDENT_PARAGRAPHS_IN_TABLE */

              output_paragraph_offset--;
              kill_self_indent (default_indentation_increment + 1);

              /* Add item's argument to the line. */
              filling_enabled = 0;
              if (item_func && *item_func)
                execute_string ("%s{%s}", item_func, rest_of_line);
              else
                execute_string ("%s", rest_of_line);

              if (current_insertion_type () == ftable)
                execute_string ("%cfindex %s\n", COMMAND_PREFIX, rest_of_line);
              else if (current_insertion_type () == vtable)
                execute_string ("%cvindex %s\n", COMMAND_PREFIX, rest_of_line);

              /* Start a new line, and let start_paragraph ()
                 do the indenting of it for you. */
              close_single_paragraph ();
              indented_fill = filling_enabled = 1;
              last_item_output_position = output_position;
            }
        }
      free (rest_of_line);
    }
  else
    {
    no_insertion:
      line_error (_("%c%s found outside of an insertion block"),
                  COMMAND_PREFIX, command);
    }
}

void
cm_itemx (void)
{
  itemx_flag++;
  cm_item ();
  itemx_flag--;
}

int headitem_flag = 0;

void
cm_headitem (void)
{
  headitem_flag = 1;
  cm_item ();
}
