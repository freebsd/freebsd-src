/* cmds.c -- Texinfo commands.
   $Id: cmds.c,v 1.69 2002/02/09 00:54:51 karl Exp $

   Copyright (C) 1998, 99, 2000, 01 Free Software Foundation, Inc.

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
#include "files.h"
#include "footnote.h"
#include "insertion.h"
#include "lang.h"
#include "macro.h"
#include "makeinfo.h"
#include "node.h"
#include "sectioning.h"
#include "toc.h"
#include "xml.h"

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif


void insert_self (), insert_space (), cm_ignore_line (), cm_ignore_arg ();

void
  cm_TeX (), cm_acronym (), cm_asterisk (), cm_b (), cm_bullet (), cm_cite (),
  cm_code (), cm_copyright (), cm_ctrl (), cm_dfn (), cm_dircategory (),
  cm_direntry (), cm_dmn (), cm_dots (), cm_emph (), cm_enddots (), cm_i (),
  cm_image (), cm_kbd (), cm_key (), cm_no_op (), 
  cm_novalidate (), cm_not_fixed_width (), cm_r (),
  cm_strong (), cm_var (), cm_sc (), cm_w (), cm_email (), cm_url (),
  cm_verb (), cm_documentdescription ();

void
  cm_anchor (), cm_node (), cm_menu (), cm_xref (), cm_ftable (),
  cm_vtable (), cm_pxref (), cm_inforef (), cm_uref (), cm_email (),
  cm_quotation (), cm_display (), cm_smalldisplay (), cm_itemize (),
  cm_enumerate (), cm_tab (), cm_table (), cm_itemx (), cm_noindent (),
  cm_setfilename (), cm_br (), cm_sp (), cm_page (), cm_group (),
  cm_center (), cm_ref (), cm_include (), cm_bye (), cm_item (), cm_end (),
  cm_kindex (), cm_cindex (), cm_findex (), cm_pindex (), cm_vindex (),
  cm_tindex (), cm_synindex (), cm_printindex (), cm_minus (),
  cm_example (), cm_smallexample (), cm_smalllisp (), cm_lisp (),
  cm_format (), cm_smallformat (), cm_exdent (), cm_defindex (),
  cm_defcodeindex (), cm_result (), cm_expansion (), cm_equiv (),
  cm_print (), cm_error (), cm_point (), cm_today (), cm_flushleft (),
  cm_flushright (), cm_finalout (), cm_cartouche (), cm_detailmenu (),
  cm_multitable (), cm_settitle (), cm_titlefont (), cm_tt (),
  cm_verbatim (), cm_verbatiminclude (), cm_titlepage ();

/* Conditionals. */
void cm_set (), cm_clear (), cm_ifset (), cm_ifclear ();
void cm_value (), cm_ifeq ();

/* Options. */
static void cm_paragraphindent (), cm_exampleindent ();

/* Internals. */
static void cm_obsolete ();

/* A random string.  */
static const char small_tag[] = "small";

/* The dispatch table.  */
COMMAND command_table[] = {
  { "\t", insert_space, NO_BRACE_ARGS },
  { "\n", insert_space, NO_BRACE_ARGS },
  { " ", insert_space, NO_BRACE_ARGS },
  { "!", insert_self, NO_BRACE_ARGS },
  { "\"", cm_accent_umlaut, MAYBE_BRACE_ARGS },
  { "'", cm_accent_acute, MAYBE_BRACE_ARGS },
  { "*", cm_asterisk, NO_BRACE_ARGS },
  { ",", cm_accent_cedilla, MAYBE_BRACE_ARGS },
  { "-", cm_no_op, NO_BRACE_ARGS },
  { ".", insert_self, NO_BRACE_ARGS },
  { ":", cm_no_op, NO_BRACE_ARGS },
  { "=", cm_accent, MAYBE_BRACE_ARGS },
  { "?", insert_self, NO_BRACE_ARGS },
  { "@", insert_self, NO_BRACE_ARGS },
  { "^", cm_accent_hat, MAYBE_BRACE_ARGS },
  { "`", cm_accent_grave, MAYBE_BRACE_ARGS },
  { "{", insert_self, NO_BRACE_ARGS },
  { "|", cm_no_op, NO_BRACE_ARGS },
  { "}", insert_self, NO_BRACE_ARGS },
  { "~", cm_accent_tilde, MAYBE_BRACE_ARGS },
  { "AA", cm_special_char, BRACE_ARGS },
  { "AE", cm_special_char, BRACE_ARGS },
  { "H", cm_accent, MAYBE_BRACE_ARGS },
  { "L", cm_special_char, BRACE_ARGS },
  { "O", cm_special_char, BRACE_ARGS },
  { "OE", cm_special_char, BRACE_ARGS },
  { "TeX", cm_TeX, BRACE_ARGS },
  { "aa", cm_special_char, BRACE_ARGS },
  { "acronym", cm_acronym, BRACE_ARGS },
  { "ae", cm_special_char, BRACE_ARGS },
  { "afivepaper", cm_ignore_line, NO_BRACE_ARGS },
  { "afourlatex", cm_ignore_line, NO_BRACE_ARGS },
  { "afourpaper", cm_ignore_line, NO_BRACE_ARGS },
  { "afourwide", cm_ignore_line, NO_BRACE_ARGS },
  { "alias", cm_alias, NO_BRACE_ARGS },
  { "anchor", cm_anchor, BRACE_ARGS },
  { "appendix", cm_appendix, NO_BRACE_ARGS },
  { "appendixsection", cm_appendixsec, NO_BRACE_ARGS },
  { "appendixsec", cm_appendixsec, NO_BRACE_ARGS },
  { "appendixsubsec", cm_appendixsubsec, NO_BRACE_ARGS },
  { "appendixsubsubsec", cm_appendixsubsubsec, NO_BRACE_ARGS },
  { "asis", cm_no_op, BRACE_ARGS },
  { "b", cm_b, BRACE_ARGS },
  { "bullet", cm_bullet, BRACE_ARGS },
  { "bye", cm_bye, NO_BRACE_ARGS },
  { "c", cm_ignore_line, NO_BRACE_ARGS },
  { "cartouche", cm_cartouche, NO_BRACE_ARGS },
  { "center", cm_center, NO_BRACE_ARGS },
  { "centerchap", cm_unnumbered, NO_BRACE_ARGS },
  { "chapheading", cm_chapheading, NO_BRACE_ARGS },
  { "chapter", cm_chapter, NO_BRACE_ARGS },
  { "cindex", cm_cindex, NO_BRACE_ARGS },
  { "cite", cm_cite, BRACE_ARGS },
  { "clear", cm_clear, NO_BRACE_ARGS },
  { "code", cm_code, BRACE_ARGS },
  { "command", cm_code, BRACE_ARGS },
  { "comment", cm_ignore_line, NO_BRACE_ARGS },
  { "contents", cm_contents, NO_BRACE_ARGS },
  { "copyright", cm_copyright, BRACE_ARGS },
  { "ctrl", cm_obsolete, BRACE_ARGS },
  { "defcodeindex", cm_defcodeindex, NO_BRACE_ARGS },
  { "defcv", cm_defun, NO_BRACE_ARGS },
  { "defcvx", cm_defun, NO_BRACE_ARGS },
  { "deffn", cm_defun, NO_BRACE_ARGS },
  { "deffnx", cm_defun, NO_BRACE_ARGS },
  { "defindex", cm_defindex, NO_BRACE_ARGS },
  { "definfoenclose", cm_definfoenclose, NO_BRACE_ARGS },
  { "defivar", cm_defun, NO_BRACE_ARGS },
  { "defivarx", cm_defun, NO_BRACE_ARGS },
  { "defmac", cm_defun, NO_BRACE_ARGS },
  { "defmacx", cm_defun, NO_BRACE_ARGS },
  { "defmethod", cm_defun, NO_BRACE_ARGS },
  { "defmethodx", cm_defun, NO_BRACE_ARGS },
  { "defop", cm_defun, NO_BRACE_ARGS },
  { "defopt", cm_defun, NO_BRACE_ARGS },
  { "defoptx", cm_defun, NO_BRACE_ARGS },
  { "defopx", cm_defun, NO_BRACE_ARGS },
  { "defspec", cm_defun, NO_BRACE_ARGS },
  { "defspecx", cm_defun, NO_BRACE_ARGS },
  { "deftp", cm_defun, NO_BRACE_ARGS },
  { "deftpx", cm_defun, NO_BRACE_ARGS },
  { "deftypefn", cm_defun, NO_BRACE_ARGS },
  { "deftypefnx", cm_defun, NO_BRACE_ARGS },
  { "deftypefun", cm_defun, NO_BRACE_ARGS },
  { "deftypefunx", cm_defun, NO_BRACE_ARGS },
  { "deftypeivar", cm_defun, NO_BRACE_ARGS },
  { "deftypeivarx", cm_defun, NO_BRACE_ARGS },
  { "deftypemethod", cm_defun, NO_BRACE_ARGS },
  { "deftypemethodx", cm_defun, NO_BRACE_ARGS },
  { "deftypeop", cm_defun, NO_BRACE_ARGS },
  { "deftypeopx", cm_defun, NO_BRACE_ARGS },
  { "deftypevar", cm_defun, NO_BRACE_ARGS },
  { "deftypevarx", cm_defun, NO_BRACE_ARGS },
  { "deftypevr", cm_defun, NO_BRACE_ARGS },
  { "deftypevrx", cm_defun, NO_BRACE_ARGS },
  { "defun", cm_defun, NO_BRACE_ARGS },
  { "defunx", cm_defun, NO_BRACE_ARGS },
  { "defvar", cm_defun, NO_BRACE_ARGS },
  { "defvarx", cm_defun, NO_BRACE_ARGS },
  { "defvr", cm_defun, NO_BRACE_ARGS },
  { "defvrx", cm_defun, NO_BRACE_ARGS },
  { "detailmenu", cm_detailmenu, NO_BRACE_ARGS },
  { "dfn", cm_dfn, BRACE_ARGS },
  { "dircategory", cm_dircategory, NO_BRACE_ARGS },
  { "direntry", cm_direntry, NO_BRACE_ARGS },
  { "display", cm_display, NO_BRACE_ARGS },
  { "dmn", cm_no_op, BRACE_ARGS },
  { "documentdescription", cm_documentdescription, NO_BRACE_ARGS },
  { "documentencoding", cm_documentencoding, NO_BRACE_ARGS },
  { "documentlanguage", cm_documentlanguage, NO_BRACE_ARGS },
  { "dotaccent", cm_accent, MAYBE_BRACE_ARGS },
  { "dotless", cm_dotless, BRACE_ARGS },
  { "dots", cm_dots, BRACE_ARGS },
  { "email", cm_email, BRACE_ARGS },
  { "emph", cm_emph, BRACE_ARGS },
  { "end", cm_end, NO_BRACE_ARGS },
  { "enddots", cm_enddots, BRACE_ARGS },
  { "enumerate", cm_enumerate, NO_BRACE_ARGS },
  { "env", cm_code, BRACE_ARGS },
  { "equiv", cm_equiv, BRACE_ARGS },
  { "error", cm_error, BRACE_ARGS },
  { "example", cm_example, NO_BRACE_ARGS },
  { "exampleindent", cm_exampleindent, NO_BRACE_ARGS },
  { "exclamdown", cm_special_char, BRACE_ARGS },
  { "exdent", cm_exdent, NO_BRACE_ARGS },
  { "expansion", cm_expansion, BRACE_ARGS },
  { "file", cm_code, BRACE_ARGS },
  { "finalout", cm_no_op, NO_BRACE_ARGS },
  { "findex", cm_findex, NO_BRACE_ARGS },
  { "flushleft", cm_flushleft, NO_BRACE_ARGS },
  { "flushright", cm_flushright, NO_BRACE_ARGS },
  { "footnote", cm_footnote, NO_BRACE_ARGS}, /* self-arg eater */
  { "footnotestyle", cm_footnotestyle, NO_BRACE_ARGS },
  { "format", cm_format, NO_BRACE_ARGS },
  { "ftable", cm_ftable, NO_BRACE_ARGS },
  { "group", cm_group, NO_BRACE_ARGS },
  { "heading", cm_heading, NO_BRACE_ARGS },
  { "headings", cm_ignore_line, NO_BRACE_ARGS },
  { "html", cm_html, NO_BRACE_ARGS },
  { "hyphenation", cm_ignore_arg, BRACE_ARGS },
  { "i", cm_i, BRACE_ARGS },
  { "ifclear", cm_ifclear, NO_BRACE_ARGS },
  { "ifeq", cm_ifeq, NO_BRACE_ARGS },
  { "ifhtml", cm_ifhtml, NO_BRACE_ARGS },
  { "ifinfo", cm_ifinfo, NO_BRACE_ARGS },
  { "ifnothtml", cm_ifnothtml, NO_BRACE_ARGS },
  { "ifnotinfo", cm_ifnotinfo, NO_BRACE_ARGS },
  { "ifnottex", cm_ifnottex, NO_BRACE_ARGS },
  { "ifset", cm_ifset, NO_BRACE_ARGS },
  { "iftex", cm_iftex, NO_BRACE_ARGS },
  { "ignore", command_name_condition, NO_BRACE_ARGS },
  { "image", cm_image, BRACE_ARGS },
  { "include", cm_include, NO_BRACE_ARGS },
  { "inforef", cm_inforef, BRACE_ARGS },
  { "item", cm_item, NO_BRACE_ARGS },
  { "itemize", cm_itemize, NO_BRACE_ARGS },
  { "itemx", cm_itemx, NO_BRACE_ARGS },
  { "kbd", cm_kbd, BRACE_ARGS },
  { "kbdinputstyle", cm_ignore_line, NO_BRACE_ARGS },
  { "key", cm_key, BRACE_ARGS },
  { "kindex", cm_kindex, NO_BRACE_ARGS },
  { "l", cm_special_char, BRACE_ARGS },
  { "lisp", cm_lisp, NO_BRACE_ARGS },
  { "lowersections", cm_lowersections, NO_BRACE_ARGS },
  { "macro", cm_macro, NO_BRACE_ARGS },
  { "majorheading", cm_majorheading, NO_BRACE_ARGS },
  { "math", cm_no_op, BRACE_ARGS },
  { "menu", cm_menu, NO_BRACE_ARGS },
  { "minus", cm_minus, BRACE_ARGS },
  { "multitable", cm_multitable, NO_BRACE_ARGS },
  { "need", cm_ignore_line, NO_BRACE_ARGS },
  { "node", cm_node, NO_BRACE_ARGS },
  { "noindent", cm_noindent, NO_BRACE_ARGS },
  { "noindent", cm_novalidate, NO_BRACE_ARGS },
  { "nwnode", cm_node, NO_BRACE_ARGS },
  { "o", cm_special_char, BRACE_ARGS },
  { "oe", cm_special_char, BRACE_ARGS },
  { "option", cm_code, BRACE_ARGS },
  { "page", cm_no_op, NO_BRACE_ARGS },
  { "pagesizes", cm_ignore_line, NO_BRACE_ARGS },
  { "paragraphindent", cm_paragraphindent, NO_BRACE_ARGS },
  { "pindex", cm_pindex, NO_BRACE_ARGS },
  { "point", cm_point, BRACE_ARGS },
  { "pounds", cm_special_char, BRACE_ARGS },
  { "print", cm_print, BRACE_ARGS },
  { "printindex", cm_printindex, NO_BRACE_ARGS },
  { "pxref", cm_pxref, BRACE_ARGS },
  { "questiondown", cm_special_char, BRACE_ARGS },
  { "quotation", cm_quotation, NO_BRACE_ARGS },
  { "r", cm_r, BRACE_ARGS },
  { "raisesections", cm_raisesections, NO_BRACE_ARGS },
  { "ref", cm_ref, BRACE_ARGS },
  { "refill", cm_no_op, NO_BRACE_ARGS },
  { "result", cm_result, BRACE_ARGS },
  { "ringaccent", cm_accent, MAYBE_BRACE_ARGS },
  { "rmacro", cm_rmacro, NO_BRACE_ARGS },
  { "samp", cm_code, BRACE_ARGS },
  { "sc", cm_sc, BRACE_ARGS },
  { "section", cm_section, NO_BRACE_ARGS },
  { "set", cm_set, NO_BRACE_ARGS },
  { "setchapternewpage", cm_ignore_line, NO_BRACE_ARGS },
  { "setchapterstyle", cm_obsolete, NO_BRACE_ARGS },
  { "setcontentsaftertitlepage", cm_no_op, NO_BRACE_ARGS },
  { "setfilename", cm_setfilename, NO_BRACE_ARGS },
  { "setshortcontentsaftertitlepage", cm_no_op, NO_BRACE_ARGS },
  { "settitle", cm_settitle, NO_BRACE_ARGS },
  { "shortcontents", cm_shortcontents, NO_BRACE_ARGS },
  { "shorttitlepage", cm_ignore_line, NO_BRACE_ARGS },
  { "smallbook", cm_ignore_line, NO_BRACE_ARGS },
  { "smalldisplay", cm_smalldisplay, NO_BRACE_ARGS },
  { "smallexample", cm_smallexample, NO_BRACE_ARGS },
  { "smallformat", cm_smallformat, NO_BRACE_ARGS },
  { "smalllisp", cm_smalllisp, NO_BRACE_ARGS },
  { "sp", cm_sp, NO_BRACE_ARGS },
  { "ss", cm_special_char, BRACE_ARGS },
  { "strong", cm_strong, BRACE_ARGS },
  { "subheading", cm_subheading, NO_BRACE_ARGS },
  { "subsection", cm_subsection, NO_BRACE_ARGS },
  { "subsubheading", cm_subsubheading, NO_BRACE_ARGS },
  { "subsubsection", cm_subsubsection, NO_BRACE_ARGS },
  { "summarycontents", cm_shortcontents, NO_BRACE_ARGS },
  { "syncodeindex", cm_synindex, NO_BRACE_ARGS },
  { "synindex", cm_synindex, NO_BRACE_ARGS },
  { "t", cm_tt, BRACE_ARGS },
  { "tab", cm_tab, NO_BRACE_ARGS },
  { "table", cm_table, NO_BRACE_ARGS },
  { "tex", cm_tex, NO_BRACE_ARGS },
  { "tieaccent", cm_accent, MAYBE_BRACE_ARGS },
  { "tindex", cm_tindex, NO_BRACE_ARGS },
  { "titlefont", cm_titlefont, BRACE_ARGS },
  { "titlepage", command_name_condition, NO_BRACE_ARGS },
  { "today", cm_today, BRACE_ARGS },
  { "top", cm_top, NO_BRACE_ARGS  },
  { "u", cm_accent, MAYBE_BRACE_ARGS },
  { "ubaraccent", cm_accent, MAYBE_BRACE_ARGS },
  { "udotaccent", cm_accent, MAYBE_BRACE_ARGS },
  { "unmacro", cm_unmacro, NO_BRACE_ARGS },
  { "unnumbered", cm_unnumbered, NO_BRACE_ARGS },
  { "unnumberedsec", cm_unnumberedsec, NO_BRACE_ARGS },
  { "unnumberedsubsec", cm_unnumberedsubsec, NO_BRACE_ARGS },
  { "unnumberedsubsubsec", cm_unnumberedsubsubsec, NO_BRACE_ARGS },
  { "uref", cm_uref, BRACE_ARGS },
  { "url", cm_url, BRACE_ARGS },
  { "v", cm_accent, MAYBE_BRACE_ARGS },
  { "value", cm_value, BRACE_ARGS },
  { "var", cm_var, BRACE_ARGS },
  { "verb", cm_verb, NO_BRACE_ARGS },
  { "verbatim", cm_verbatim, NO_BRACE_ARGS },
  { "verbatiminclude", cm_verbatiminclude, NO_BRACE_ARGS },
  { "vindex", cm_vindex, NO_BRACE_ARGS },
  { "vtable", cm_vtable, NO_BRACE_ARGS },
  { "w", cm_w, BRACE_ARGS },
  { "xref", cm_xref, BRACE_ARGS },

  /* Deprecated commands.  These used to be for italics.  */
  { "iappendix", cm_ideprecated, NO_BRACE_ARGS },
  { "iappendixsec", cm_ideprecated, NO_BRACE_ARGS },
  { "iappendixsection", cm_ideprecated, NO_BRACE_ARGS },
  { "iappendixsubsec", cm_ideprecated, NO_BRACE_ARGS },
  { "iappendixsubsubsec", cm_ideprecated, NO_BRACE_ARGS },
  { "ichapter", cm_ideprecated, NO_BRACE_ARGS },
  { "isection", cm_ideprecated, NO_BRACE_ARGS },
  { "isubsection", cm_ideprecated, NO_BRACE_ARGS },
  { "isubsubsection", cm_ideprecated, NO_BRACE_ARGS },
  { "iunnumbered", cm_ideprecated, NO_BRACE_ARGS },
  { "iunnumberedsec", cm_ideprecated, NO_BRACE_ARGS },
  { "iunnumberedsubsec", cm_ideprecated, NO_BRACE_ARGS },
  { "iunnumberedsubsubsec", cm_ideprecated, NO_BRACE_ARGS },

  /* Now @include does what this was used to. */
  { "infoinclude", cm_obsolete, NO_BRACE_ARGS },
  { "titlespec", cm_obsolete, NO_BRACE_ARGS },

  { NULL, NULL, NO_BRACE_ARGS }
};

/* The bulk of the Texinfo commands. */

/* Commands which insert their own names. */
void
insert_self (arg)
    int arg;
{
  if (arg == START)
    add_word (command);
}

void
insert_space (arg)
    int arg;
{
  if (arg == START)
    {
      if (xml && !docbook)
	xml_insert_entity ("space");
      else
	add_char (' ');
    }
}

/* Force a line break in the output. */
void
cm_asterisk ()
{
  if (html)
    add_word ("<br>");
  else if (xml && !docbook)
    xml_insert_entity ("linebreak");
  else if (docbook) 
    xml_asterisk ();
  else
    {
      close_single_paragraph ();
      cm_noindent ();
    }
}

/* Insert ellipsis. */
void
cm_dots (arg)
     int arg;
{
  if (arg == START)
    {
      if (xml && !docbook)
	xml_insert_entity ("dots");
      else if (docbook)
	xml_insert_entity ("hellip");
      else
	add_word (html ? "<small>...</small>" : "...");
    }
}

/* Insert ellipsis for sentence end. */
void
cm_enddots (arg)
     int arg;
{
  if (arg == START)
    {
      if (xml && !docbook)
	xml_insert_entity ("enddots");
      else if (docbook)
	{
	  xml_insert_entity ("hellip");
	  add_char ('.');
	}
      else
	add_word (html ? "<small>...</small>." : "....");
    }
}

void
cm_bullet (arg)
     int arg;
{
  if (arg == START)
    {
      if (html)
        add_word ("&#149;");
      else if (xml && !docbook)
	xml_insert_entity ("bullet");
      else if (docbook)
	xml_insert_entity ("bull");
      else
        add_char ('*');
    }
}

void
cm_minus (arg)
     int arg;
{
  if (arg == START)
    {
      if (xml)
	xml_insert_entity ("minus");
      else
	add_char ('-');
    }
}

/* Insert "TeX". */
void
cm_TeX (arg)
     int arg;
{
  if (arg == START)
    {
      if (xml && ! docbook)
	xml_insert_entity ("tex");
      else
	add_word ("TeX");
    }
}

/* Copyright symbol.  */
void
cm_copyright (arg)
    int arg;
{
  if (arg == START)
    {
    if (html)
      add_word ("&copy;");
    else if (xml && !docbook)
      xml_insert_entity ("copyright");
    else if (docbook)
      xml_insert_entity ("copy");
    else
      add_word ("(C)");
    }
}

void
cm_today (arg)
     int arg;
{
  static char *months[12] =
    { N_("January"), N_("February"), N_("March"), N_("April"), N_("May"),
      N_("June"), N_("July"), N_("August"), N_("September"), N_("October"),
      N_("November"), N_("December") };
  if (arg == START)
    {
      time_t timer = time (0);
      struct tm *ts = localtime (&timer);
      add_word_args ("%d %s %d", ts->tm_mday, _(months[ts->tm_mon]),
                     ts->tm_year + 1900);
    }
}

void
cm_acronym (arg)
     int arg;
{
  if (html)
    insert_html_tag (arg, small_tag);
  else if (xml)
    xml_insert_element (ACRONYM, arg);
}

void
cm_tt (arg)
     int arg;
{
  /* @t{} is a no-op in Info.  */
  if (html)
    insert_html_tag (arg, "tt");
  else if (xml)
    xml_insert_element (TT, arg);
}

void
cm_code (arg)
     int arg;
{
  if (xml)
    xml_insert_element (CODE, arg);
  else
    {
  extern int printing_index;

  if (arg == START)
    {
      in_fixed_width_font++;

      if (html)
        insert_html_tag (arg, "code");
      else if (!printing_index)
        add_char ('`');
    }
  else if (html)
    insert_html_tag (arg, "code");
  else
    {
      if (!printing_index)
        add_meta_char ('\'');
    }
    }
}

void
cm_kbd (arg)
     int arg;
{
  if (xml)
    xml_insert_element (KBD, arg);
  else if (html)
    { /* Seems like we should increment in_fixed_width_font for Info
         format too, but then the quote-omitting special case gets
         confused.  Punt.  */
      if (arg == START)
        in_fixed_width_font++;
      insert_html_tag (arg, "kbd");
    }
  else
    { /* People use @kbd in an example to get the "user input" font.
         We don't want quotes in that case.  */
      if (!in_fixed_width_font)
        cm_code (arg);
    }
}

void
cm_url (arg, start, end)
{
  if (xml)
    xml_insert_element (URL, arg);
  else if (html)
    {
      if (arg == START)
        add_word ("&lt;<code>");
      else
	add_word ("</code>&gt;");
    }
  else
    if (arg == START)
      add_word ("<");
    else
      add_word (">");
}

void
cm_key (arg)
     int arg;
{
  if (xml)
    xml_insert_element (KEY, arg);
  else if (html)
    add_word (arg == START ? "&lt;" : "&gt;");
  else
    add_char (arg == START ? '<' : '>');
}

/* Handle a command that switches to a non-fixed-width font.  */
void
not_fixed_width (arg)
     int arg;
{
  if (arg == START)
    in_fixed_width_font = 0;
}

/* @var in makeinfo just uppercases the text. */
void
cm_var (arg, start_pos, end_pos)
     int arg, start_pos, end_pos;
{
  if (xml)
    xml_insert_element (VAR, arg);
  else
    {
  not_fixed_width (arg);

  if (html)
    insert_html_tag (arg, "var");
  else if (arg == END)
    {
      while (start_pos < end_pos)
        {
          unsigned char c = output_paragraph[start_pos];
          if (strchr ("[](),", c))
            warning (_("unlikely character %c in @var"), c);
          output_paragraph[start_pos] = coerce_to_upper (c);
          start_pos++;
        }
    }
    }
}

void
cm_sc (arg, start_pos, end_pos)
     int arg, start_pos, end_pos;
{
  if (xml)
    xml_insert_element (SC, arg);
  else
    {
  not_fixed_width (arg);

  if (arg == START)
    {
      if (html)
	insert_html_tag (arg, small_tag);
    }
  else
    {
      int all_upper;

      if (html)
        start_pos += sizeof (small_tag) + 2 - 1; /* skip <small> */

      /* Avoid the warning below if there's no text inside @sc{}, or
         when processing menus under --no-headers.  */
      all_upper = start_pos < end_pos;

      while (start_pos < end_pos)
        {
          unsigned char c = output_paragraph[start_pos];
          if (!isupper (c))
            all_upper = 0;
          output_paragraph[start_pos] = coerce_to_upper (c);
          start_pos++;
        }
      if (all_upper)
        warning (_("@sc argument all uppercase, thus no effect"));
        
      if (html)
	insert_html_tag (arg, small_tag);
    }
    }
}

void
cm_dfn (arg, position)
     int arg, position;
{
  if (xml)
    xml_insert_element (DFN, arg);
  else
    {
  if (html)
    insert_html_tag (arg, "dfn");
  else if (arg == START)
    add_char ('"');
  else
    add_meta_char ('"');
    }
}

void
cm_emph (arg)
     int arg;
{
  if (xml)
    xml_insert_element (EMPH, arg);
  else if (html)
    insert_html_tag (arg, "em");
  else
    add_char ('_');
}

void
cm_verb (arg)
     int arg;
{
  int character;
  int delimiter;
  int seen_end = 0;

  in_fixed_width_font++;
  /* are these necessary ? */
  last_char_was_newline = 0;

  if (html)
    add_word ("<pre>");

  if (input_text_offset < input_text_length)
    {
      character = curchar ();
      if (character == '{')
	input_text_offset++;
      else
	line_error (_("`{' expected, but saw `%c'"), character);
    }
    
  if (input_text_offset < input_text_length)
    {
      delimiter = curchar ();
      input_text_offset++;
    }

  while (input_text_offset < input_text_length)
    {
      character = curchar ();

      if (character == '\n')
        line_number++;
      /*
	Assume no newlines in END_VERBATIM
      */
      else if (character == delimiter)
	{
	  seen_end = 1;
	  input_text_offset++;
	  break;
	}

      add_char (character);
      input_text_offset++;
    }

  if (!seen_end)
    warning (_("end of file inside verb block"));
  
  if (input_text_offset < input_text_length)
    {
      character = curchar ();
      if (character == '}')
	input_text_offset++;
      else
	line_error (_("`}' expected, but saw `%c'"), character);
    }

  if (html)
    add_word ("</pre>");
}

void
cm_strong (arg, position)
     int arg, position;
{
  if (xml)
    xml_insert_element (STRONG, arg);
  else if (html)
    insert_html_tag (arg, "strong");
  else
    add_char ('*');
}

void
cm_cite (arg, position)
     int arg, position;
{
  if (xml)
    xml_insert_element (CITE, arg);        
  else if (html)
    insert_html_tag (arg, "cite");
  else
    {
      if (arg == START)
        add_char ('`');
      else
        add_char ('\'');
    }
}

/* No highlighting, but argument switches fonts.  */
void
cm_not_fixed_width (arg, start, end)
     int arg, start, end;
{
  if (xml)
    xml_insert_element (NOTFIXEDWIDTH, arg);
  not_fixed_width (arg);
}

void
cm_i (arg)
     int arg;
{
  if (xml)
    xml_insert_element (I, arg);
  else if (html)
    insert_html_tag (arg, "i");
  else
    not_fixed_width (arg);
}

void
cm_b (arg)
     int arg;
{
  if (xml)
    xml_insert_element (B, arg);
  else if (html)
    insert_html_tag (arg, "b");
  else
    not_fixed_width (arg);
}

void
cm_r (arg)
     int arg;
{
  if (xml)
    xml_insert_element (R, arg);
  else
    {
      extern int printing_index;

      /* People use @r{} in index entries like this:
	 
      @findex foo@r{, some text}
      
      This is supposed to produce output as if the entry were saying
      "@code{foo}, some text", since the "fn" index is typeset as
      @code.  The following attempts to do the same in HTML.  Note that
      this relies on the fact that only @code bumps up the variable
      in_fixed_width_font while processing index entries in HTML mode.  */
      if (html && printing_index)
	{
	  int level = in_fixed_width_font;
	  
	  while (level--)
	    insert_html_tag (arg == START ? END : START, "code");
	}
      
      not_fixed_width (arg);
    }
}

void
cm_titlefont (arg)
     int arg;
{
  if (xml)
    xml_insert_element (TITLEFONT, arg);
  else
  not_fixed_width (arg);
}

/* Various commands are no-op's. */
void
cm_no_op ()
{
}


/* For proofing single chapters, etc.  */
void
cm_novalidate ()
{
  validating = 0;
}


/* Prevent the argument from being split across two lines. */
void
cm_w (arg, start, end)
     int arg, start, end;
{
  if (arg == START)
    non_splitting_words++;
  else
    non_splitting_words--;
}


/* Explain that this command is obsolete, thus the user shouldn't
   do anything with it. */
static void
cm_obsolete (arg, start, end)
     int arg, start, end;
{
  if (arg == START)
    warning (_("%c%s is obsolete"), COMMAND_PREFIX, command);
}


/* This says to inhibit the indentation of the next paragraph, but
   not of following paragraphs.  */
void
cm_noindent ()
{
  if (!inhibit_paragraph_indentation)
    inhibit_paragraph_indentation = -1;
}

/* I don't know exactly what to do with this.  Should I allow
   someone to switch filenames in the middle of output?  Since the
   file could be partially written, this doesn't seem to make sense.
   Another option: ignore it, since they don't *really* want to
   switch files.  Finally, complain, or at least warn.  It doesn't
   really matter, anyway, since this doesn't get executed.  */
void
cm_setfilename ()
{
  char *filename;
  get_rest_of_line (1, &filename);
  /* warning ("`@%s %s' encountered and ignored", command, filename); */
  if (xml)
    add_word_args ("<setfilename>%s</setfilename>", filename);
  free (filename);
}

void
cm_settitle ()
{
  if (xml)
    {
      xml_begin_document ();
      xml_insert_element (SETTITLE, START);
      get_rest_of_line (0, &title);
      execute_string ("%s", title);
      xml_insert_element (SETTITLE, END);
    }
  else
    get_rest_of_line (0, &title);
}


/* Ignore argument in braces.  */
void
cm_ignore_arg (arg, start_pos, end_pos)
     int arg, start_pos, end_pos;
{
  if (arg == END)
    output_paragraph_offset = start_pos;
}

/* Ignore argument on rest of line.  */
void
cm_ignore_line ()
{
  discard_until ("\n");
}

/* Insert the number of blank lines passed as argument. */
void
cm_sp ()
{
  int lines;
  char *line;

  get_rest_of_line (1, &line);

  if (sscanf (line, "%d", &lines) != 1 || lines <= 0)
    line_error (_("@sp requires a positive numeric argument, not `%s'"), line);
  else
    {
      if (xml)
	{
	  xml_insert_element_with_attribute (SP, START, "lines=\"%s\"", line);
	  /*	  insert_string (line);*/
	  xml_insert_element (SP, END);
	}
      else
	{
	  /* Must disable filling since otherwise multiple newlines is like
         multiple spaces.  Must close paragraph since that's what the
         manual says and that's what TeX does.  */
      int save_filling_enabled = filling_enabled;
      filling_enabled = 0;
      
      close_paragraph ();

      if (lines && html && !executing_string)
	html_output_head ();

      while (lines--)
	{
	  if (html)
	    insert_string ("<br><p>\n");
	  else
	    add_char ('\n');
	}

      filling_enabled = save_filling_enabled;
    }
    }
  free (line);
}

/* @dircategory LINE outputs INFO-DIR-SECTION LINE, unless --no-headers.  */ 
void
cm_dircategory ()
{
  char *line;

  if (html || docbook)
    cm_ignore_line ();
  else if (xml)
    {
      xml_insert_element (DIRCATEGORY, START);
      get_rest_of_line (1, &line);
      insert_string (line);
      free (line);
      xml_insert_element (DIRCATEGORY, END);
    }
  else
    {
      get_rest_of_line (1, &line);

      if (!no_headers && !html)
        {
          kill_self_indent (-1); /* make sure there's no indentation */
          insert_string ("INFO-DIR-SECTION ");
          insert_string (line);
          insert ('\n');
        }

      free (line);
    }
}

/* Start a new line with just this text on it.
   Then center the line of text.
   */
void
cm_center ()
{
  if (xml)
    {
      unsigned char *line;
      xml_insert_element (CENTER, START);
      get_rest_of_line (0, (char **)&line);
      execute_string ("%s", (char *)line);
      free (line);
      xml_insert_element (CENTER, END);
    }
  else
    {
  int i, start, length;
  unsigned char *line;
  int save_indented_fill = indented_fill;
  int save_filling_enabled = filling_enabled;
  int fudge_factor = 1;

  filling_enabled = indented_fill = 0;
  cm_noindent ();
  start = output_paragraph_offset;

  if (html)
    add_word ("<div align=\"center\">");

  inhibit_output_flushing ();
  get_rest_of_line (0, (char **)&line);
  execute_string ("%s", (char *)line);
  free (line);
  uninhibit_output_flushing ();
   if (html)
    add_word ("</div>");

   else
     {
       i = output_paragraph_offset - 1;
       while (i > (start - 1) && output_paragraph[i] == '\n')
	 i--;

       output_paragraph_offset = ++i;
       length = output_paragraph_offset - start;

       if (length < (fill_column - fudge_factor))
	 {
	   line = xmalloc (1 + length);
	   memcpy (line, (char *)(output_paragraph + start), length);

	   i = (fill_column - fudge_factor - length) / 2;
	   output_paragraph_offset = start;

	   while (i--)
	     insert (' ');

	   for (i = 0; i < length; i++)
	     insert (line[i]);

	   free (line);
	 }
     }

  insert ('\n');
  filling_enabled = save_filling_enabled;
  indented_fill = save_indented_fill;
    }
}

/* Show what an expression returns. */
void
cm_result (arg)
     int arg;
{
  if (arg == END)
    add_word (html ? "=&gt;" : "=>");
}

/* What an expression expands to. */
void
cm_expansion (arg)
     int arg;
{
  if (arg == END)
    add_word (html ? "==&gt;" : "==>");
}

/* Indicates two expressions are equivalent. */
void
cm_equiv (arg)
     int arg;
{
  if (arg == END)
    add_word ("==");
}

/* What an expression may print. */
void
cm_print (arg)
     int arg;
{
  if (arg == END)
    add_word ("-|");
}

/* An error signaled. */
void
cm_error (arg)
     int arg;
{
  if (arg == END)
    add_word (html ? "error--&gt;" : "error-->");
}

/* The location of point in an example of a buffer. */
void
cm_point (arg)
     int arg;
{
  if (arg == END)
    add_word ("-!-");
}

/* @exdent: Start a new line with just this text on it.
   The text is outdented one level if possible. */
void
cm_exdent ()
{
  char *line;
  int save_indent = current_indent;
  int save_in_fixed_width_font = in_fixed_width_font;

  /* Read argument  */
  get_rest_of_line (0, &line);

  /* Exdent the output.  Actually this may be a no-op.   */
  if (current_indent)
    current_indent -= default_indentation_increment;

  /* @exdent arg is supposed to be in roman.  */
  in_fixed_width_font = 0;
  
  /* The preceding newline already inserted the `current_indent'.
     Remove one level's worth.  */
  kill_self_indent (default_indentation_increment);

  if (html)
    add_word ("<br>");

  /* Can't close_single_paragraph, then we lose preceding blank lines.  */
  flush_output ();
  execute_string ("%s", line);
  free (line);

  if (html)
    add_word ("<br>");
  close_single_paragraph ();

  current_indent = save_indent;
  in_fixed_width_font = save_in_fixed_width_font;
}

/* 
  Read include-filename, process the include-file:
    verbatim_include == 0: process through reader_loop
    verbatim_include != 0: process through handle_verbatim_environment
 */
static void
handle_include (verbatim_include)
  int verbatim_include;
{
  char *filename;

  if (macro_expansion_output_stream && !executing_string)
    me_append_before_this_command ();

  close_paragraph ();
  get_rest_of_line (0, &filename);

  if (macro_expansion_output_stream && !executing_string)
    remember_itext (input_text, input_text_offset);

  pushfile ();

  /* In verbose mode we print info about including another file. */
  if (verbose_mode)
    {
      int i = 0;
      FSTACK *stack = filestack;

      for (i = 0, stack = filestack; stack; stack = stack->next, i++);

      i *= 2;

      printf ("%*s", i, "");
      printf ("%c%s `%s'\n", COMMAND_PREFIX, command, filename);
      fflush (stdout);
    }

  if (!find_and_load (filename))
    {
      extern int errno;

      popfile ();
      line_number--;

      /* /wh/bar:5: @include/@verbatiminclude `foo': No such file or dir */
      line_error ("%c%s `%s': %s", COMMAND_PREFIX, command, filename,
                  strerror (errno));

      free (filename);
      return;
    }
  else
    {
      if (macro_expansion_output_stream && !executing_string)
	remember_itext (input_text, input_text_offset);

      if (!verbatim_include)
	reader_loop ();
      else
	handle_verbatim_environment (0);
    }
  free (filename);
  popfile ();
}


/* Include file as if put in @verbatim environment */
void
cm_verbatiminclude ()
{
  handle_include (1); 
}


/* Remember this file, and move onto the next. */
void
cm_include ()
{
  handle_include (0); 
}


/* @bye: Signals end of processing.  Easy to make this happen. */

void
cm_bye ()
{
  discard_braces (); /* should not have any unclosed braces left */
  flush_output ();
  input_text_offset = input_text_length;
}

/* @paragraphindent */

static void
cm_paragraphindent ()
{
  char *arg;

  get_rest_of_line (1, &arg);
  if (set_paragraph_indent (arg) != 0)
    line_error (_("Bad argument to %c%s"), COMMAND_PREFIX, command);

  free (arg);
}

/* @exampleindent: change indentation of example-like environments.   */
static int
set_default_indentation_increment (string)
     char *string;
{
  if (strcmp (string, "asis") == 0 || strcmp (string, _("asis")) == 0)
    ;
  else if (strcmp (string, "none") == 0 || strcmp (string, _("none")) == 0)
    default_indentation_increment = 0;
  else if (sscanf (string, "%d", &default_indentation_increment) != 1)
    return -1;
  return 0;
}

static void
cm_exampleindent ()
{
  char *arg;
  
  get_rest_of_line (1, &arg);
  if (set_default_indentation_increment (arg) != 0)
    line_error (_("Bad argument to %c%s"), COMMAND_PREFIX, command);

  free (arg);
}
