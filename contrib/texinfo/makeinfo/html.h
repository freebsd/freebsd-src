/* html.h -- declarations for html-related utilities.
   $Id: html.h,v 1.2 2000/12/19 15:17:52 karl Exp $

   Copyright (C) 1999, 2000 Free Software Foundation, Inc.

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

#ifndef HTML_H
#define HTML_H

/* Nonzero if we have output the <head>.  */
extern int html_output_head_p;

/* Perform the <head> output.  */
extern void html_output_head ();

/* Escape &<>.  */
extern char *escape_string (/* char * */);

/* Open or close TAG according to START_OR_END.  */
extern void insert_html_tag (/* int start_or_end, char *tag */);

/* Output HTML <link> to NODE, plus extra ATTRIBUTES.  */
extern void add_link (/* char *node, char *attributes */);

/* Escape URL-special characters as %xy.  */
extern void add_escaped_anchor_name (/* char *name */);

/* See html.c.  */
extern void add_anchor_name (/* nodename, href */);
extern void add_url_name ( /* nodename, href */ );
extern char* nodename_to_filename ( /* nodename */ );
extern void add_nodename_to_filename ( /*nodename, href */ );

#endif /* !HTML_H */
