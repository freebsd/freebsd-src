/* xml.h -- xml output declarations.
   $Id: xml.h,v 1.3 2001/05/01 16:29:29 karl Exp $

   Copyright (C) 2001 Free Software Foundation, Inc.

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

   Written by Philippe Martin <feloy@free.fr>.  */

#ifndef XML_H
#define XML_H

/* Options. */

/* Separate index entries into divisions for each letters. */
extern int xml_index_divisions;
extern int xml_sort_index;

extern int xml_node_open;
extern int xml_no_para;
extern char *xml_node_id;
extern int xml_last_section_output_position;

enum xml_element
{
  TEXINFO=0, SETFILENAME, TITLEFONT, SETTITLE, 
  /* Node */
  NODE /* 4 */, NODENEXT, NODEPREV, NODEUP,
  /* Structuring */
  CHAPTER /* 8 */, SECTION, SUBSECTION, SUBSUBSECTION,
  TOP /* 12 */, UNNUMBERED, UNNUMBEREDSEC, UNNUMBEREDSUBSEC, UNNUMBEREDSUBSUBSEC, 
  APPENDIX /* 17 */, APPENDIXSEC, APPENDIXSUBSEC, APPENDIXSUBSUBSEC, 
  MAJORHEADING /* 21 */, CHAPHEADING, HEADING, SUBHEADING, SUBSUBHEADING,
  /* Menu */
  MENU /* 26 */, MENUENTRY, MENUTITLE, MENUCOMMENT, MENUNODE, NODENAME,
  /* -- */
  ACRONYM/* 32 */, TT, CODE, KBD, URL, KEY, VAR, SC, DFN, EMPH, STRONG, CITE, NOTFIXEDWIDTH, I, B, R, 
  TITLE, 
  IFINFO, 
  SP, CENTER,
  DIRCATEGORY,
  QUOTATION, EXAMPLE, SMALLEXAMPLE, LISP, SMALLLISP, CARTOUCHE, FORMAT, SMALLFORMAT, DISPLAY, SMALLDISPLAY,
  FOOTNOTE, 
  ITEMIZE, ITEMFUNCTION, ITEM, ENUMERATE, TABLE, TABLEITEM, TABLETERM,
  INDEXTERM, 
  XREF, XREFNODENAME, XREFINFONAME, XREFPRINTEDDESC, XREFINFOFILE, XREFPRINTEDNAME,
  INFOREF, INFOREFNODENAME, INFOREFREFNAME, INFOREFINFONAME, 
  UREF, UREFURL, UREFDESC, UREFREPLACEMENT,
  EMAIL, EMAILADDRESS, EMAILNAME,
  GROUP,
  PRINTINDEX,
  ANCHOR, 
  IMAGE,
  PRIMARY, SECONDARY, INFORMALFIGURE, MEDIAOBJECT, IMAGEOBJECT, IMAGEDATA, TEXTOBJECT, 
  INDEXENTRY, PRIMARYIE, SECONDARYIE, INDEXDIV,
  MULTITABLE, TGROUP, COLSPEC, TBODY, ENTRY, ROW,
  BOOKINFO, ABSTRACT, REPLACEABLE,
  PARA
};

extern void xml_insert_element (/* int name, int arg */);
extern char *xml_id (/* char *id */);

#endif /* XML_H */
