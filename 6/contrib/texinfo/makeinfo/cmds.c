/* cmds.c -- Texinfo commands.
   $Id: cmds.c,v 1.55 2004/12/14 00:15:36 karl Exp $

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
#include "files.h"
#include "footnote.h"
#include "html.h"
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

/* Options. */
static void cm_exampleindent (void),
     cm_firstparagraphindent (void),
     cm_paragraphindent (void),
     cm_novalidate (void);

/* Internals. */
static void cm_obsolete (int arg, int start, int end),
     not_fixed_width (int arg);

/* The dispatch table.  */
COMMAND command_table[] = {
  { "\t", insert_space, NO_BRACE_ARGS },
  { "\n", insert_space, NO_BRACE_ARGS },
  { " ", insert_space, NO_BRACE_ARGS },
  { "!", cm_punct, NO_BRACE_ARGS },
  { "\"", cm_accent_umlaut, MAYBE_BRACE_ARGS },
  { "'", cm_accent_acute, MAYBE_BRACE_ARGS },
  { "*", cm_asterisk, NO_BRACE_ARGS },
  { ",", cm_accent_cedilla, MAYBE_BRACE_ARGS },
  { "-", cm_no_op, NO_BRACE_ARGS },
  { ".", cm_punct, NO_BRACE_ARGS },
  { "/", cm_no_op, NO_BRACE_ARGS },
  { ":", cm_colon, NO_BRACE_ARGS },
  { "=", cm_accent, MAYBE_BRACE_ARGS },
  { "?", cm_punct, NO_BRACE_ARGS },
  { "@", insert_self, NO_BRACE_ARGS },
  { "\\", insert_self, NO_BRACE_ARGS },
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
  { "LaTeX", cm_LaTeX, BRACE_ARGS },
  { "O", cm_special_char, BRACE_ARGS },
  { "OE", cm_special_char, BRACE_ARGS },
  { "TeX", cm_TeX, BRACE_ARGS },
  { "aa", cm_special_char, BRACE_ARGS },
  { "abbr", cm_abbr, BRACE_ARGS },
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
  { "author", cm_author, NO_BRACE_ARGS },
  { "b", cm_b, BRACE_ARGS },
  { "bullet", cm_bullet, BRACE_ARGS },
  { "bye", cm_bye, NO_BRACE_ARGS },
  { "c", cm_comment, NO_BRACE_ARGS },
  { "caption", cm_caption, BRACE_ARGS },
  { "cartouche", cm_cartouche, NO_BRACE_ARGS },
  { "center", cm_center, NO_BRACE_ARGS },
  { "centerchap", cm_unnumbered, NO_BRACE_ARGS },
  { "chapheading", cm_chapheading, NO_BRACE_ARGS },
  { "chapter", cm_chapter, NO_BRACE_ARGS },
  { "cindex", cm_cindex, NO_BRACE_ARGS },
  { "cite", cm_cite, BRACE_ARGS },
  { "clear", cm_clear, NO_BRACE_ARGS },
  { "code", cm_code, BRACE_ARGS },
  { "comma", cm_comma, BRACE_ARGS },
  { "command", cm_code, BRACE_ARGS },
  { "comment", cm_comment, NO_BRACE_ARGS },
  { "contents", cm_contents, NO_BRACE_ARGS },
  { "copying", cm_copying, NO_BRACE_ARGS },
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
  { "deftypecv", cm_defun, NO_BRACE_ARGS },
  { "deftypecvx", cm_defun, NO_BRACE_ARGS },
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
  { "dmn", cm_dmn, BRACE_ARGS },
  { "docbook", cm_docbook, NO_BRACE_ARGS },
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
  { "euro", cm_special_char, BRACE_ARGS },
  { "evenfooting", cm_ignore_line, NO_BRACE_ARGS },
  { "evenheading", cm_ignore_line, NO_BRACE_ARGS },
  { "everyfooting", cm_ignore_line, NO_BRACE_ARGS },
  { "everyheading", cm_ignore_line, NO_BRACE_ARGS },
  { "example", cm_example, NO_BRACE_ARGS },
  { "exampleindent", cm_exampleindent, NO_BRACE_ARGS },
  { "exclamdown", cm_special_char, BRACE_ARGS },
  { "exdent", cm_exdent, NO_BRACE_ARGS },
  { "expansion", cm_expansion, BRACE_ARGS },
  { "file", cm_code, BRACE_ARGS },
  { "finalout", cm_no_op, NO_BRACE_ARGS },
  { "findex", cm_findex, NO_BRACE_ARGS },
  { "firstparagraphindent", cm_firstparagraphindent, NO_BRACE_ARGS },
  { "float", cm_float, NO_BRACE_ARGS },
  { "flushleft", cm_flushleft, NO_BRACE_ARGS },
  { "flushright", cm_flushright, NO_BRACE_ARGS },
  { "footnote", cm_footnote, NO_BRACE_ARGS}, /* self-arg eater */
  { "footnotestyle", cm_footnotestyle, NO_BRACE_ARGS },
  { "format", cm_format, NO_BRACE_ARGS },
  { "ftable", cm_ftable, NO_BRACE_ARGS },
  { "group", cm_group, NO_BRACE_ARGS },
  { "heading", cm_heading, NO_BRACE_ARGS },
  { "headings", cm_ignore_line, NO_BRACE_ARGS },
  { "headitem", cm_headitem, NO_BRACE_ARGS },
  { "html", cm_html, NO_BRACE_ARGS },
  { "hyphenation", cm_ignore_arg, BRACE_ARGS },
  { "i", cm_i, BRACE_ARGS },
  { "ifclear", cm_ifclear, NO_BRACE_ARGS },
  { "ifeq", cm_ifeq, NO_BRACE_ARGS },
  { "ifdocbook", cm_ifdocbook, NO_BRACE_ARGS },
  { "ifhtml", cm_ifhtml, NO_BRACE_ARGS },
  { "ifinfo", cm_ifinfo, NO_BRACE_ARGS },
  { "ifnotdocbook", cm_ifnotdocbook, NO_BRACE_ARGS },
  { "ifnothtml", cm_ifnothtml, NO_BRACE_ARGS },
  { "ifnotinfo", cm_ifnotinfo, NO_BRACE_ARGS },
  { "ifnotplaintext", cm_ifnotplaintext, NO_BRACE_ARGS },
  { "ifnottex", cm_ifnottex, NO_BRACE_ARGS },
  { "ifnotxml", cm_ifnotxml, NO_BRACE_ARGS },
  { "ifplaintext", cm_ifplaintext, NO_BRACE_ARGS },
  { "ifset", cm_ifset, NO_BRACE_ARGS },
  { "iftex", cm_iftex, NO_BRACE_ARGS },
  { "ifxml", cm_ifxml, NO_BRACE_ARGS },
  { "ignore", command_name_condition, NO_BRACE_ARGS },
  { "image", cm_image, BRACE_ARGS },
  { "include", cm_include, NO_BRACE_ARGS },
  { "indent", cm_indent, NO_BRACE_ARGS },
  { "indicateurl", cm_indicate_url, BRACE_ARGS },
  { "inforef", cm_inforef, BRACE_ARGS },
  { "insertcopying", cm_insert_copying, NO_BRACE_ARGS },
  { "item", cm_item, NO_BRACE_ARGS },
  { "itemize", cm_itemize, NO_BRACE_ARGS },
  { "itemx", cm_itemx, NO_BRACE_ARGS },
  { "kbd", cm_kbd, BRACE_ARGS },
  { "kbdinputstyle", cm_ignore_line, NO_BRACE_ARGS },
  { "key", cm_key, BRACE_ARGS },
  { "kindex", cm_kindex, NO_BRACE_ARGS },
  { "l", cm_special_char, BRACE_ARGS },
  { "lisp", cm_lisp, NO_BRACE_ARGS },
  { "listoffloats", cm_listoffloats, NO_BRACE_ARGS },
  { "lowersections", cm_lowersections, NO_BRACE_ARGS },
  { "macro", cm_macro, NO_BRACE_ARGS },
  { "majorheading", cm_majorheading, NO_BRACE_ARGS },
  { "math", cm_math, BRACE_ARGS },
  { "menu", cm_menu, NO_BRACE_ARGS },
  { "minus", cm_minus, BRACE_ARGS },
  { "multitable", cm_multitable, NO_BRACE_ARGS },
  { "need", cm_ignore_line, NO_BRACE_ARGS },
  { "node", cm_node, NO_BRACE_ARGS },
  { "noindent", cm_noindent_cmd, NO_BRACE_ARGS },
  { "novalidate", cm_novalidate, NO_BRACE_ARGS },
  { "nwnode", cm_node, NO_BRACE_ARGS },
  { "o", cm_special_char, BRACE_ARGS },
  { "oddfooting", cm_ignore_line, NO_BRACE_ARGS },
  { "oddheading", cm_ignore_line, NO_BRACE_ARGS },
  { "oe", cm_special_char, BRACE_ARGS },
  { "option", cm_code, BRACE_ARGS },
  { "ordf", cm_special_char, BRACE_ARGS },
  { "ordm", cm_special_char, BRACE_ARGS },
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
  { "registeredsymbol", cm_registeredsymbol, BRACE_ARGS },
  { "result", cm_result, BRACE_ARGS },
  { "ringaccent", cm_accent, MAYBE_BRACE_ARGS },
  { "rmacro", cm_rmacro, NO_BRACE_ARGS },
  { "samp", cm_code, BRACE_ARGS },
  { "sansserif", cm_sansserif, BRACE_ARGS },
  { "sc", cm_sc, BRACE_ARGS },
  { "section", cm_section, NO_BRACE_ARGS },
  { "set", cm_set, NO_BRACE_ARGS },
  { "setchapternewpage", cm_ignore_line, NO_BRACE_ARGS },
  { "setchapterstyle", cm_obsolete, NO_BRACE_ARGS },
  { "setcontentsaftertitlepage", cm_no_op, NO_BRACE_ARGS },
  { "setfilename", cm_setfilename, NO_BRACE_ARGS },
  { "setshortcontentsaftertitlepage", cm_no_op, NO_BRACE_ARGS },
  { "settitle", cm_settitle, NO_BRACE_ARGS },
  { "shortcaption", cm_caption, BRACE_ARGS },
  { "shortcontents", cm_contents, NO_BRACE_ARGS },
  { "shorttitlepage", cm_ignore_line, NO_BRACE_ARGS },
  { "slanted", cm_slanted, BRACE_ARGS },
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
  { "subtitle", cm_titlepage_cmds, NO_BRACE_ARGS },
  { "summarycontents", cm_contents, NO_BRACE_ARGS },
  { "syncodeindex", cm_synindex, NO_BRACE_ARGS },
  { "synindex", cm_synindex, NO_BRACE_ARGS },
  { "t", cm_tt, BRACE_ARGS },
  { "tab", cm_tab, NO_BRACE_ARGS },
  { "table", cm_table, NO_BRACE_ARGS },
  { "tex", cm_tex, NO_BRACE_ARGS },
  { "tie", cm_tie, BRACE_ARGS },
  { "tieaccent", cm_accent, MAYBE_BRACE_ARGS },
  { "tindex", cm_tindex, NO_BRACE_ARGS },
  { "title", cm_titlepage_cmds, NO_BRACE_ARGS },
  { "titlefont", cm_titlefont, BRACE_ARGS },
  { "titlepage", cm_titlepage, NO_BRACE_ARGS },
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
  { "url", cm_uref, BRACE_ARGS },
  { "v", cm_accent, MAYBE_BRACE_ARGS },
  { "value", cm_value, BRACE_ARGS },
  { "var", cm_var, BRACE_ARGS },
  { "verb", cm_verb, NO_BRACE_ARGS },
  { "verbatim", cm_verbatim, NO_BRACE_ARGS },
  { "verbatiminclude", cm_verbatiminclude, NO_BRACE_ARGS },
  { "vindex", cm_vindex, NO_BRACE_ARGS },
  { "vtable", cm_vtable, NO_BRACE_ARGS },
  { "vskip", cm_ignore_line, NO_BRACE_ARGS },
  { "w", cm_w, BRACE_ARGS },
  { "xml", cm_xml, NO_BRACE_ARGS },
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
insert_self (int arg)
{
  if (arg == START)
    add_word (command);
}

void
insert_space (int arg)
{
  if (arg == START)
    {
      if (xml && !docbook)
        xml_insert_entity ("space");
      else
        add_char (' ');
    }
}

/* Insert a comma.  Useful when a literal , would break our parsing of
   multiple arguments.  */
void
cm_comma (int arg)
{
  if (arg == START)
    add_char (',');
}


/* Force a line break in the output. */
void
cm_asterisk (void)
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
cm_dots (int arg)
{
  if (arg == START)
    {
      if (xml && !docbook)
        xml_insert_entity ("dots");
      else if (docbook)
        xml_insert_entity ("hellip");
      else
	if (html && !in_fixed_width_font)
	  insert_string ("<small class=\"dots\">...</small>");
	else
	  add_word ("...");
    }
}

/* Insert ellipsis for sentence end. */
void
cm_enddots (int arg)
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
	if (html && !in_fixed_width_font)
	  insert_string ("<small class=\"enddots\">....</small>");
	else
	  add_word ("....");
    }
}

void
cm_bullet (int arg)
{
  if (arg == START)
    {
      if (html)
        add_word ("&bull;");
      else if (xml && !docbook)
	xml_insert_entity ("bullet");
      else if (docbook)
	xml_insert_entity ("bull");
      else
        add_char ('*');
    }
}

void
cm_minus (int arg)
{
  if (arg == START)
    {
      if (xml)
	xml_insert_entity ("minus");
      else if (html)
        add_word ("&minus;");
      else
	add_char ('-');
    }
}

/* Formatting a dimension unit.  */
void
cm_dmn (int arg)
{
  if (html)
    insert_html_tag_with_attribute (arg, "span", "class=\"dmn\"");
  else if (docbook)
    /* No units in docbook yet.  */
    ;
  else if (xml)
    xml_insert_element (DIMENSION, arg);
}

/* Insert "TeX". */
void
cm_TeX (int arg)
{
  static int last_position;

  if (arg == START)
    {
      if (xml)
	xml_insert_entity ("tex");
      else
	add_word ("TeX");

      last_position = output_paragraph_offset;
    }
  else if (last_position != output_paragraph_offset)
    {
      warning (_("arguments to @%s ignored"), command);
      output_paragraph_offset = last_position;
    }
}

/* Insert "LaTeX".  */
void
cm_LaTeX (int arg)
{
  static int last_position;

  if (arg == START)
    {
      if (xml)
        xml_insert_entity ("latex");
      else
        add_word ("LaTeX");

      last_position = output_paragraph_offset;
    }
  else if (last_position != output_paragraph_offset)
    {
      warning (_("arguments to @%s ignored"), command);
      output_paragraph_offset = last_position;
    }
}

/* Copyright symbol.  */
void
cm_copyright (int arg)
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

/* Registered symbol.  */
void
cm_registeredsymbol (int arg)
{
  if (arg == START)
    {
      if (html)
        add_word ("&reg;");
      else if (docbook)
        xml_insert_entity ("reg");
      else if (xml && !docbook)
        xml_insert_entity ("registered");
      else
        add_word ("(R)");
    }
}

void
cm_today (int arg)
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
cm_comment (void)
{
  /* For HTML, do not output comments before HTML header is written,
     otherwise comments before @settitle cause an empty <title> in the
     header.  */
  if ((html && html_output_head_p) || xml)
    {
      char *line;
      get_rest_of_line (0, &line);

      if (strlen (line) > 0)
        {
          int save_inhibit_indentation = inhibit_paragraph_indentation;
          int save_paragraph_is_open = paragraph_is_open;
          int save_escape_html = escape_html;
          int save_xml_no_para = xml_no_para;
          int i;

          inhibit_paragraph_indentation = 1;
          escape_html = 0;
          xml_no_para = 1;

          /* @c and @comment can appear between @item and @itemx,
             @deffn and @deffnx.  */
          xml_dont_touch_items_defs++;

          /* Use insert for HTML, and XML when indentation is enabled.
             For Docbook, use add_char.  */
          if (xml && xml_indentation_increment > 0
              && output_paragraph[output_paragraph_offset-1] != '\n')
            insert ('\n');

          /* Crunch double hyphens in comments.  */
          add_html_block_elt ("<!-- ");
          for (i = 0; i < strlen (line); i++)
            if (line[i] != '-' || (i && line[i-1] != '-'))
              add_char (line[i]);
          add_word (" -->");

          if (html)
            add_char ('\n');

          inhibit_paragraph_indentation = save_inhibit_indentation;
          paragraph_is_open = save_paragraph_is_open;
          escape_html = save_escape_html;
          xml_no_para = save_xml_no_para;
          xml_dont_touch_items_defs--;
        }

      free (line);
    }
  else
    cm_ignore_line ();
}



/* We keep acronyms with two arguments around, to be able to refer to them
   later with only one argument.  */
static ACRONYM_DESC *acronyms_stack = NULL;

static void
cm_acronym_or_abbr (int arg, int is_abbr)
{
  char *aa, *description;
  unsigned len;

  /* We do everything at START.  */
  if (arg == END)
    return;

  get_until_in_braces (",", &aa);
  if (input_text[input_text_offset] == ',')
    input_text_offset++;
  get_until_in_braces ("}", &description);

  canon_white (aa);
  canon_white (description);

  /* If not enclosed in braces, strip after comma to be compatible
     with texinfo.tex.  */
  if (description[0] != '{' && strchr (description, ',') != NULL)
    {
      int i = 0;
      while (description[i] != ',')
        i++;
      /* For now, just terminate the string at comma.  */
      description[i] = 0;
    }

  /* Get description out of braces.  */
  if (description[0] == '{')
    description++;

  len = strlen (description);
  if (len && description[len-1] == '}')
    description[len-1] = 0;

  /* Save new description.  */
  if (strlen (description) > 0)
    {
      ACRONYM_DESC *new = xmalloc (sizeof (ACRONYM_DESC));

      new->acronym = xstrdup (aa);
      new->description = xstrdup (description);
      new->next = acronyms_stack;
      acronyms_stack = new;
    }

  if (html)
    {
      add_word (is_abbr ? "<abbr" : "<acronym");

      if (strlen (description) > 0)
        add_word_args (" title=\"%s\"", text_expansion (description));
      else if (acronyms_stack)
        {
          /* No second argument, get from previous.  Search order is from
             last to first defined, so we get the most recent version of
             the description.  */
          ACRONYM_DESC *temp = acronyms_stack;

          while (temp)
            {
              if (STREQ (aa, temp->acronym)
                  && strlen (temp->description) > 0)
                {
                  add_word_args (" title=\"%s\"",
                                 text_expansion (temp->description));
                  break;
                }
              temp = temp->next;
            }
        }

      add_char ('>');
      execute_string ("%s", aa);
      add_word (is_abbr ? "</abbr>" : "</acronym>");
    }
  else if (docbook)
    {
      xml_insert_element (is_abbr ? ABBREV : ACRONYM, START);
      execute_string ("%s", aa);
      xml_insert_element (is_abbr ? ABBREV : ACRONYM, END);
    }
  else if (xml)
    {
      xml_insert_element (is_abbr ? ABBREV : ACRONYM, START);

      xml_insert_element (is_abbr ? ABBREVWORD : ACRONYMWORD, START);
      execute_string ("%s", aa);
      xml_insert_element (is_abbr ? ABBREVWORD : ACRONYMWORD, END);

      if (strlen (description) > 0)
        {
          xml_insert_element (is_abbr ? ABBREVDESC : ACRONYMDESC, START);
          execute_string ("%s", description);
          xml_insert_element (is_abbr ? ABBREVDESC : ACRONYMDESC, END);
        }

      xml_insert_element (is_abbr ? ABBREV : ACRONYM, END);
    }
  else
    execute_string ("%s", aa);

  /* Put description into parenthesis after the acronym for all outputs
     except XML.  */
  if (strlen (description) > 0 && (!xml || docbook))
    add_word_args (" (%s)", description);
}

void
cm_acronym (int arg)
{
  cm_acronym_or_abbr (arg, 0);
}

void
cm_abbr (int arg)
{
  cm_acronym_or_abbr (arg, 1);
}

void
cm_tt (int arg)
{
  /* @t{} is a no-op in Info.  */
  if (html)
    insert_html_tag (arg, "tt");
  else if (xml)
    xml_insert_element (TT, arg);
}

void
cm_code (int arg)
{
  if (arg == START)
    in_fixed_width_font++;

  if (xml)
    {
      if (STREQ (command, "command"))
	xml_insert_element (COMMAND_TAG, arg);
      else if (STREQ (command, "env"))
	xml_insert_element (ENV, arg);
      else if (STREQ (command, "file"))
	xml_insert_element (FILE_TAG, arg);
      else if (STREQ (command, "option"))
	xml_insert_element (OPTION, arg);
      else if (STREQ (command, "samp"))
        {
          if (docbook && arg == START)
            {
              /* Even though @samp is in_fixed_width_font, it
                 should always start a paragraph.  Unfortunately,
                 in_fixed_width_font inhibits that.  */
              xml_start_para ();
              xml_insert_entity ("lsquo");
            }
          xml_insert_element (SAMP, arg);
          if (docbook && arg == END)
            xml_insert_entity ("rsquo");
        }
      else
	xml_insert_element (CODE, arg);
    }
  else if (html)
    {
      if (STREQ (command, "code"))
        insert_html_tag (arg, "code");
      else
        { /* Use <samp> tag in general to get typewriter.  */
          if (arg == START)
            { /* If @samp specifically, add quotes a la TeX output.  */
              if (STREQ (command, "samp")) add_char ('`');
              add_word ("<samp>");
            }
          insert_html_tag_with_attribute (arg, "span", "class=\"%s\"",command);
          if (arg == END)
            {
              add_word ("</samp>");
              if (STREQ (command, "samp")) add_char ('\'');
            }
        }
    }
  else
    {
      extern int printing_index;

      if (!printing_index)
        {
          if (arg == START)
            add_char ('`');
          else
            add_meta_char ('\'');
        }
    }
}

void
cm_kbd (int arg)
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

/* Just show a url (http://example.org/..., for example), don't link to it.  */
void
cm_indicate_url (int arg, int start, int end)
{
  if (xml)
    xml_insert_element (URL, arg);
  else if (html)
    {
      if (arg == START)
        add_word ("&lt;");
      insert_html_tag (arg, "code");
      if (arg != START)
        add_word ("&gt;");
    }
  else
    if (arg == START)
      add_word ("<");
    else
      add_word (">");
}

void
cm_key (int arg)
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
not_fixed_width (int arg)
{
  if (arg == START)
    in_fixed_width_font = 0;
}

/* @var in makeinfo just uppercases the text. */
void
cm_var (int arg, int start_pos, int end_pos)
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
cm_sc (int arg, int start_pos, int end_pos)
{
  if (xml)
    xml_insert_element (SC, arg);
  else
    {
      not_fixed_width (arg);

      if (arg == START)
        {
          if (html)
            insert_html_tag_with_attribute (arg, "span", "class=\"sc\"");
        }
      else
        {
          int all_upper;

          if (html)
            start_pos += sizeof ("<span class=\"sc\">") - 1; /* skip <span> */

          /* Avoid the warning below if there's no text inside @sc{}, or
             when processing menus under --no-headers.  */
          all_upper = start_pos < end_pos;

          while (start_pos < end_pos)
            {
              unsigned char c = output_paragraph[start_pos];
              if (!isupper (c))
                all_upper = 0;
              if (!html)
                output_paragraph[start_pos] = coerce_to_upper (c);
              start_pos++;
            }
          if (all_upper)
            warning (_("@sc argument all uppercase, thus no effect"));

          if (html)
            insert_html_tag (arg, "span");
        }
    }
}

void
cm_dfn (int arg, int position)
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
cm_emph (int arg)
{
  if (xml)
    xml_insert_element (EMPH, arg);
  else if (html)
    insert_html_tag (arg, "em");
  else
    add_char ('_');
}

void
cm_verb (int arg)
{
  int character;
  int delimiter = 0; /* avoid warning */
  int seen_end = 0;

  in_fixed_width_font++;
  /* are these necessary ? */
  last_char_was_newline = 0;

  if (html)
    add_word ("<tt>");

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
        {
          line_number++;
          if (html)
            add_word ("<br>\n");
        }

      else if (html && character == '<')
        add_word ("&lt;");

      else if (html && character == '&')
        add_word ("&amp;");

      else if (character == delimiter && input_text[input_text_offset+1] == '}')
	{ /* Assume no newlines in END_VERBATIM. */
	  seen_end = 1;
	  input_text_offset++;
	  break;
	}

      else
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
    add_word ("</tt>");

  in_fixed_width_font--;
}


void
cm_strong (int arg, int start_pos, int end_pos)
{
  if (docbook && arg == START)
    xml_insert_element_with_attribute (B, arg, "role=\"bold\"");
  else if (xml)
    xml_insert_element (STRONG, arg);
  else if (html)
    insert_html_tag (arg, "strong");
  else
    add_char ('*');
  
  if (!xml && !html && !docbook && !no_headers
      && arg == END
      && end_pos - start_pos >= 6
      && (STRNCASEEQ ((char *) output_paragraph + start_pos, "*Note:", 6)
          || STRNCASEEQ ((char *) output_paragraph + start_pos, "*Note ", 6)))
    {
      /* Translators: "Note:" is literal here and should not be
         translated.  @strong{Nota}, say, does not cause the problem.  */
      warning (_("@strong{Note...} produces a spurious cross-reference in Info; reword to avoid that"));
      /* Adjust the output to avoid writing the bad xref.  */
      output_paragraph[start_pos + 5] = '_';
    }
}

void
cm_cite (int arg, int position)
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
cm_not_fixed_width (int arg, int start, int end)
{
  if (xml)
    xml_insert_element (NOTFIXEDWIDTH, arg);
  not_fixed_width (arg);
}

void
cm_i (int arg)
{
  /* Make use of <lineannotation> of Docbook, if we are
     inside an @example or similar.  */
  extern int printing_index;
  if (docbook && !filling_enabled && !printing_index)
    xml_insert_element (LINEANNOTATION, arg);
  else if (xml)
    xml_insert_element (I, arg);
  else if (html)
    insert_html_tag (arg, "i");
  else
    not_fixed_width (arg);
}

void
cm_slanted (int arg)
{
  /* Make use of <lineannotation> of Docbook, if we are
     inside an @example or similar.  */
  extern int printing_index;
  if (docbook && !filling_enabled && !printing_index)
    xml_insert_element (LINEANNOTATION, arg);
  else if (xml)
    xml_insert_element (SLANTED, arg);
  else if (html)
    insert_html_tag (arg, "i");
  else
    not_fixed_width (arg);
}

void
cm_b (int arg)
{
  /* See cm_i comments.  */
  extern int printing_index;
  if (docbook && !filling_enabled && !printing_index)
    xml_insert_element (LINEANNOTATION, arg);
  else if (docbook && arg == START)
    xml_insert_element_with_attribute (B, arg, "role=\"bold\"");
  else if (xml)
    xml_insert_element (B, arg);
  else if (html)
    insert_html_tag (arg, "b");
  else
    not_fixed_width (arg);
}

void
cm_r (int arg)
{
  /* See cm_i comments.  */
  extern int printing_index;
  if (docbook && !filling_enabled && !printing_index)
    xml_insert_element (LINEANNOTATION, arg);
  else if (xml)
    xml_insert_element (R, arg);
  else if (html)
    insert_html_tag_with_attribute (arg, "span", "class=\"roman\"");
  else
    not_fixed_width (arg);
}

void
cm_sansserif (int arg)
{
  /* See cm_i comments.  */
  extern int printing_index;
  if (docbook && !filling_enabled && !printing_index)
    xml_insert_element (LINEANNOTATION, arg);
  else if (xml)
    xml_insert_element (SANSSERIF, arg);
  else if (html)
    insert_html_tag_with_attribute (arg, "span", "class=\"sansserif\"");
  else
    not_fixed_width (arg);
}

void
cm_titlefont (int arg)
{
  if (xml)
    xml_insert_element (TITLEFONT, arg);
  else
   {
     not_fixed_width (arg);
     if (html)
	{
	  html_title_written = 1; /* suppress title from @settitle */
	  if (arg == START)
	    add_word ("<h1 class=\"titlefont\">");
	  else
	    add_word ("</h1>\n");
	}
   }
}


/* Unfortunately, we cannot interpret @math{} contents like TeX does.  We just
   pass them through.  */
void
cm_math (int arg)
{
  if (xml && !docbook)
    xml_insert_element (MATH, arg);
}

/* Various commands are no-op's. */
void
cm_no_op (void)
{
}


/* For proofing single chapters, etc.  */
void
cm_novalidate (void)
{
  validating = 0;
}


/* Prevent the argument from being split across two lines. */
void
cm_w (int arg)
{
  if (arg == START)
    non_splitting_words++;
  else
    {
      if (docbook || html || xml)
        /* This is so @w{$}Log$ doesn't end up as <dollar>Log<dollar>
           in the output.  */
        insert_string ("<!-- /@w -->");
        
      non_splitting_words--;
    }
}


/* An unbreakable word space.  Same as @w{ } for makeinfo, but different
   for TeX (the space stretches and stretches, and does not inhibit
   hyphenation).  */
void
cm_tie (int arg)
{
  if (arg == START)
    {
      cm_w (START);
      add_char (' ');
    }
  else
    cm_w (END);
}

/* Explain that this command is obsolete, thus the user shouldn't
   do anything with it. */
static void
cm_obsolete (int arg, int start, int end)
{
  if (arg == START)
    warning (_("%c%s is obsolete"), COMMAND_PREFIX, command);
}


/* Inhibit the indentation of the next paragraph, but not of following
   paragraphs.  */
void
cm_noindent (void)
{
  if (!inhibit_paragraph_indentation)
    inhibit_paragraph_indentation = -1;
}

void
cm_noindent_cmd (void)
{
  cm_noindent ();
  xml_no_indent = 1;
  skip_whitespace_and_newlines();

  if (xml)
    xml_start_para ();
  else if (html && !paragraph_is_open)
    add_html_block_elt ("<p class=\"noindent\">");
  else
    {
      paragraph_is_open = 0;
      start_paragraph ();
    }
}

/* Force indentation of the next paragraph. */
void
cm_indent (void)
{
  inhibit_paragraph_indentation = 0;
  xml_no_indent = 0;
  skip_whitespace_and_newlines();
  
  if (xml)
    xml_start_para ();
  else if (html && !paragraph_is_open)
    add_html_block_elt ("<p class=\"indent\">");
  else
    start_paragraph ();
}

/* I don't know exactly what to do with this.  Should I allow
   someone to switch filenames in the middle of output?  Since the
   file could be partially written, this doesn't seem to make sense.
   Another option: ignore it, since they don't really want to
   switch files.  Finally, complain, or at least warn.  It doesn't
   really matter, anyway, since this doesn't get executed.  */
void
cm_setfilename (void)
{
  char *filename;
  get_rest_of_line (1, &filename);
  /* warning ("`@%s %s' encountered and ignored", command, filename); */
  if (xml)
    add_word_args ("<setfilename>%s</setfilename>", filename);
  free (filename);
}

void
cm_settitle (void)
{
  if (xml)
    {
      xml_begin_document (current_output_filename);
      xml_insert_element (SETTITLE, START);
      xml_in_book_title = 1;
      get_rest_of_line (0, &title);
      execute_string ("%s", title);
      xml_in_book_title = 0;
      xml_insert_element (SETTITLE, END);
    }
  else
    get_rest_of_line (0, &title);
}


/* Ignore argument in braces.  */
void
cm_ignore_arg (int arg, int start_pos, int end_pos)
{
  if (arg == END)
    output_paragraph_offset = start_pos;
}

/* Ignore argument on rest of line.  */
void
cm_ignore_line (void)
{
  discard_until ("\n");
}

/* Insert the number of blank lines passed as argument. */
void
cm_sp (void)
{
  int lines;
  char *line;

  /* Due to tricky stuff in execute_string(), @value{} can't be expanded.
     So there is really no reason to enable expansion for @sp parameters.  */
  get_rest_of_line (0, &line);

  if (sscanf (line, "%d", &lines) != 1 || lines <= 0)
    line_error (_("@sp requires a positive numeric argument, not `%s'"), line);
  else
    {
      if (xml)
	{
          /* @sp can appear between @item and @itemx, @deffn and @deffnx.  */
          xml_dont_touch_items_defs++;
	  xml_insert_element_with_attribute (SP, START, "lines=\"%s\"", line);
	  /*	  insert_string (line);*/
	  xml_insert_element (SP, END);
          xml_dont_touch_items_defs--;
	}
      else
        {
          /* Must disable filling since otherwise multiple newlines is like
             multiple spaces.  Must close paragraph since that's what the
             manual says and that's what TeX does.  */
          int save_filling_enabled = filling_enabled;
          filling_enabled = 0;

          /* close_paragraph generates an extra blank line.  */
          close_single_paragraph ();

          if (lines && html && !executing_string)
            html_output_head ();

          if (html)
            add_html_block_elt ("<pre class=\"sp\">\n");

          while (lines--)
            add_char ('\n');

          if (html)
            add_html_block_elt ("</pre>\n");

          filling_enabled = save_filling_enabled;
        }
    }
  free (line);
}

/* @dircategory LINE outputs INFO-DIR-SECTION LINE, unless --no-headers.  */ 
void
cm_dircategory (void)
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
cm_center (void)
{
  if (xml)
    {
      char *line;
      xml_insert_element (CENTER, START);
      get_rest_of_line (0, &line);
      execute_string ("%s", line);
      free (line);
      xml_insert_element (CENTER, END);
    }
  else
    {
      int i, start, length;
      char *line;
      int save_indented_fill = indented_fill;
      int save_filling_enabled = filling_enabled;
      int fudge_factor = 1;

      filling_enabled = indented_fill = 0;
      cm_noindent ();
      start = output_paragraph_offset;

      if (html)
        add_html_block_elt ("<div align=\"center\">");

      inhibit_output_flushing ();
      get_rest_of_line (0, &line);
      execute_string ("%s", line);
      free (line);
      uninhibit_output_flushing ();
      if (html)
        add_html_block_elt ("</div>");

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
      close_single_paragraph ();
      if (looking_at("\n"))
        insert ('\n');
    }
}

/* Show what an expression returns. */
void
cm_result (int arg)
{
  if (arg == END)
    add_word (html ? "=&gt;" : "=>");
}

/* What an expression expands to. */
void
cm_expansion (int arg)
{
  if (arg == END)
    add_word (html ? "==&gt;" : "==>");
}

/* Indicates two expressions are equivalent. */
void
cm_equiv (int arg)
{
  if (arg == END)
    add_word ("==");
}

/* What an expression may print. */
void
cm_print (int arg)
{
  if (arg == END)
    add_word ("-|");
}

/* An error signaled. */
void
cm_error (int arg)
{
  if (arg == END)
    add_word (html ? "error--&gt;" : "error-->");
}

/* The location of point in an example of a buffer. */
void
cm_point (int arg)
{
  if (arg == END)
    add_word ("-!-");
}

/* @exdent: Start a new line with just this text on it.
   The text is outdented one level if possible. */
void
cm_exdent (void)
{
  char *line;
  int save_indent = current_indent;
  int save_in_fixed_width_font = in_fixed_width_font;

  /* Read argument.  */
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
  else if (docbook)
    xml_insert_element (LINEANNOTATION, START);
  else if (xml)
    xml_insert_element (EXDENT, START);

  /* Can't close_single_paragraph, then we lose preceding blank lines.  */
  flush_output ();
  execute_string ("%s", line);
  free (line);

  if (html)
    add_word ("<br>");
  else if (xml)
    {
      xml_insert_element (docbook ? LINEANNOTATION : EXDENT, END);
      insert ('\n');
    }

  close_single_paragraph ();

  current_indent = save_indent;
  in_fixed_width_font = save_in_fixed_width_font;
  if (!xml)
    start_paragraph ();
}

/* 
  Read include-filename, process the include-file:
    verbatim_include == 0: process through reader_loop
    verbatim_include != 0: process through handle_verbatim_environment
 */
static void
handle_include (int verbatim_include)
{
  char *arg, *filename;

  if (macro_expansion_output_stream && !executing_string)
    me_append_before_this_command ();

  if (!insertion_stack)
    close_paragraph ();  /* No blank lines etc. if not at outer level.  */
    
  get_rest_of_line (0, &arg);
  /* We really only want to expand @value, but it's easier to just do
     everything.  TeX will only work with @value.  */
  filename = text_expansion (arg);
  free (arg);

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

  if (!find_and_load (filename, 1))
    {
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
cm_verbatiminclude (void)
{
  handle_include (1); 
}


/* Remember this file, and move onto the next. */
void
cm_include (void)
{
  handle_include (0); 
}


/* @bye: Signals end of processing.  Easy to make this happen. */

void
cm_bye (void)
{
  discard_braces (); /* should not have any unclosed braces left */
  input_text_offset = input_text_length;
}

/* @paragraphindent */

static void
cm_paragraphindent (void)
{
  char *arg;

  get_rest_of_line (1, &arg);
  if (set_paragraph_indent (arg) != 0)
    line_error (_("Bad argument to %c%s"), COMMAND_PREFIX, command);

  free (arg);
}


/* @exampleindent: change indentation of example-like environments.   */
static int
set_example_indentation_increment (char *string)
{
  if (strcmp (string, "asis") == 0 || strcmp (string, _("asis")) == 0)
    ;
  else if (strcmp (string, "none") == 0 || strcmp (string, _("none")) == 0)
    example_indentation_increment = 0;
  else if (sscanf (string, "%d", &example_indentation_increment) != 1)
    return -1;
  return 0;
}

static void
cm_exampleindent (void)
{
  char *arg;
  
  get_rest_of_line (1, &arg);
  if (set_example_indentation_increment (arg) != 0)
    line_error (_("Bad argument to @%s"), command);

  if (input_text[input_text_offset] == '\n')
    close_single_paragraph ();

  free (arg);
}


/* @firstparagraphindent: suppress indentation in first paragraphs after
   headings. */
static int
set_firstparagraphindent (char *string)
{
  if (STREQ (string, "insert") || STREQ (string, _("insert")))
    do_first_par_indent = 1;
  else if (STREQ (string, "none") || STREQ (string, _("none")))
    do_first_par_indent = 0;
  else
    return -1;
  return 0;
}

static void
cm_firstparagraphindent (void)
{
  char *arg;

  get_rest_of_line (1, &arg);
  if (set_firstparagraphindent (arg) != 0)
    line_error (_("Bad argument to %c%s"), COMMAND_PREFIX, command);

  free (arg);
}

/* For DocBook and XML, produce &period; for `.@:'. This gives the processing
   software a fighting chance to treat it specially by not adding extra space.
  
   Do this also for ?, !, and :.  */
void
cm_colon (void)
{
  if (xml)
    {
      if (strchr (".?!:", input_text[input_text_offset-3]) != NULL)
        {
          /* Erase literal character that's there, except `>', which is
             part of the XML tag.  */
          if (output_paragraph[output_paragraph_offset-1] != '>')
            output_paragraph_offset--;

          switch (input_text[input_text_offset-3])
            {
            case '.':
              xml_insert_entity ("period");
              break;
            case '?':
              xml_insert_entity ("quest");
              break;
            case '!':
              xml_insert_entity ("excl");
              break;
            case ':':
              xml_insert_entity ("colon");
              break;
            }
        }
    }
}

/* Ending sentences explicitly.  Currently, only outputs entities for XML
   output, for other formats it calls insert_self.  */
void
cm_punct (int arg)
{
  if (xml && !docbook)
    {
      switch (input_text[input_text_offset-1])
        {
        case '.':
          xml_insert_entity ("eosperiod");
          break;
        case '?':
          xml_insert_entity ("eosquest");
          break;
        case '!':
          xml_insert_entity ("eosexcl");
          break;
        }
    }
  else
    {
      insert_self (arg);
    }
}
