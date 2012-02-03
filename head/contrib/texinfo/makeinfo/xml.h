/* xml.h -- xml output declarations.
   $Id: xml.h,v 1.24 2004/11/26 00:48:35 karl Exp $

   Copyright (C) 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

   Originally written by Philippe Martin <feloy@free.fr>.  */

#ifndef XML_H
#define XML_H

/* Options. */

/* Separate index entries into divisions for each letters. */
extern int xml_index_divisions;
extern int xml_sort_index;

extern int xml_no_indent;

extern int xml_node_open;
extern int xml_no_para;
extern char *xml_node_id;
extern int xml_last_section_output_position;

extern int xml_in_xref_token;
extern int xml_in_bookinfo;
extern int xml_in_book_title;
extern int xml_in_abstract;

/* Non-zero if we are handling an element that can appear between
   @item and @itemx, @deffn and @deffnx.  */
extern int xml_dont_touch_items_defs;

/* Non-zero if whitespace in the source document should be kept as-is.  */
extern int xml_keep_space;

enum xml_element
{
  TEXINFO=0, SETFILENAME, TITLEFONT, SETTITLE, DOCUMENTDESCRIPTION,
  /* Node */
  NODE, NODENEXT, NODEPREV, NODEUP,
  /* Structuring */
  CHAPTER, SECTION, SUBSECTION, SUBSUBSECTION,
  TOP, UNNUMBERED, UNNUMBEREDSEC, UNNUMBEREDSUBSEC,
    UNNUMBEREDSUBSUBSEC,  
  APPENDIX, APPENDIXSEC, APPENDIXSUBSEC, APPENDIXSUBSUBSEC, 
  MAJORHEADING, CHAPHEADING, HEADING, SUBHEADING, SUBSUBHEADING,
  /* Titlepage */
  TITLEPAGE, AUTHOR, BOOKTITLE, BOOKSUBTITLE,
  /* Menu */
  MENU, DETAILMENU, MENUENTRY, MENUTITLE, MENUCOMMENT, MENUNODE,
  NODENAME,
  /* -- */
  ACRONYM, ACRONYMWORD, ACRONYMDESC,
  ABBREV, ABBREVWORD, ABBREVDESC,
  TT, CODE, COMMAND_TAG, ENV, FILE_TAG, OPTION, SAMP, KBD, URL, KEY,
  VAR, SC, DFN, EMPH, STRONG, CITE, NOTFIXEDWIDTH, I, B, R, SLANTED, SANSSERIF,
  EXDENT,
  TITLE, 
  IFINFO, 
  SP, CENTER,
  DIRCATEGORY,
  QUOTATION, EXAMPLE, SMALLEXAMPLE, LISP, SMALLLISP, CARTOUCHE,
    COPYING, FORMAT, SMALLFORMAT, DISPLAY, SMALLDISPLAY, VERBATIM,
  FOOTNOTE, LINEANNOTATION,
  TIP, NOTE, IMPORTANT, WARNING, CAUTION,
  ITEMIZE, ITEMFUNCTION, ITEM, ENUMERATE, TABLE, TABLEITEM, TABLETERM,
  INDEXTERM, 
  MATH, DIMENSION,
  XREF, XREFNODENAME, XREFINFONAME, XREFPRINTEDDESC, XREFINFOFILE,
    XREFPRINTEDNAME, 
  INFOREF, INFOREFNODENAME, INFOREFREFNAME, INFOREFINFONAME, 
  UREF, UREFURL, UREFDESC, UREFREPLACEMENT,
  EMAIL, EMAILADDRESS, EMAILNAME,
  GROUP, FLOAT, FLOATTYPE, FLOATPOS, CAPTION, SHORTCAPTION,
  FLOATTABLE, FLOATFIGURE, FLOATEXAMPLE, FLOATCARTOUCHE,
  PRINTINDEX, LISTOFFLOATS,
  ANCHOR, 
  IMAGE, INLINEIMAGE, IMAGEALTTEXT,
  PRIMARY, SECONDARY, INFORMALFIGURE, MEDIAOBJECT, IMAGEOBJECT,
    IMAGEDATA, TEXTOBJECT,  
  INDEXENTRY, PRIMARYIE, SECONDARYIE, INDEXDIV,
  MULTITABLE, TGROUP, COLSPEC, THEAD, TBODY, ENTRY, ROW,
  BOOKINFO, ABSTRACT, REPLACEABLE, ENVAR, COMMENT, FUNCTION, LEGALNOTICE,
  CONTENTS, SHORTCONTENTS, DOCUMENTLANGUAGE,
  SETVALUE, CLEARVALUE,
  DEFINITION, DEFINITIONTERM, DEFINITIONITEM,
  DEFCATEGORY, DEFFUNCTION, DEFVARIABLE, DEFPARAM, DEFDELIMITER, DEFTYPE,
  DEFPARAMTYPE, DEFDATATYPE, DEFCLASS, DEFCLASSVAR, DEFOPERATION,
  PARA
};

extern void xml_add_char (int character),
  xml_asterisk (void),
  xml_insert_element (int elt, int arg),
  xml_insert_entity (char *entity_name),
  xml_insert_footnote (char *note),
  xml_insert_quotation (char *type, int arg),
  xml_insert_indexentry (char *entry, char *node),
  xml_insert_indexterm (char *indexterm, char *index),
  xml_insert_docbook_image (char *name_arg),
  xml_synindex (char *from, char *to),
  xml_start_para (void),
  xml_end_para (void),
  xml_begin_document (char *output_filename),
  xml_end_document (void),
  xml_start_menu_entry (char *tem),
  xml_end_menu (void),
  xml_end_current_element (void),
  xml_open_section (int level, char *name),
  xml_close_sections (int level),
  xml_begin_node (void),
  xml_begin_index (void),
  xml_end_index (void),
  xml_begin_multitable (int ncolumns, int *column_widths),
  xml_end_multitable (void),
  xml_end_multitable_row (int first_row),
  xml_end_multitable_column (void),
  xml_begin_table (int type, char *item_function),
  xml_end_table (int type),
  xml_begin_item (void),
  xml_begin_table_item (void),
  xml_continue_table_item (void),
  xml_begin_enumerate (char *enum_arg),
  xml_end_enumerate (void),
  xml_begin_docbook_float (int elt);

extern char *xml_id (char *id);

extern void xml_begin_definition (void),
  xml_end_definition (void),
  xml_process_defun_args (char **defun_args, int auto_var_p),
  xml_begin_def_term (int base_type, const char *category,
      char *defined_name, char *type_name, char *type_name2),
  xml_end_def_term (void);

extern int xml_current_stack_index (void),
  xml_element (char *name);

#if defined (VA_FPRINTF) && __STDC__
void xml_insert_element_with_attribute (int elt, int arg, char *format, ...);
#else
void xml_insert_element_with_attribute ();
#endif

#endif /* XML_H */
