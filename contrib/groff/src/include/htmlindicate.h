// -*- C++ -*-
/* Copyright (C) 2000, 2001 Free Software Foundation, Inc.
     Written by Gaius Mulley <gaius@glam.ac.uk>

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#ifndef HTMLINDICATE_H
#define HTMLINDICATE_H

/*
 *  graphic_start - emit a html graphic start indicator, but only
 *                  if one has not already been issued.
 *
 *                  The boolean, is_inline, should be:
 *
 *                  FALSE if this is called via EQ, TS, PS, and
 *                  TRUE if issued via delim $$  $ x over y $ etc.
 */
extern void graphic_start(int is_inline);

/*
 *  graphic_end - emit a html graphic end indicator, but only
 *                if a corresponding matching graphic-start has
 *                been issued.
 *
 */
extern void graphic_end();

/*
 *  html_begin_suppress - suppresses output for the html device
 *                        and resets the min/max registers for -Tps
 *
 *                        The boolean, is_inline, should be:
 *
 *                        FALSE if this is called via EQ, TS, PS, and
 *                        TRUE if issued via delim $$  $ x over y $ etc.
 */
extern void html_begin_suppress(int is_inline);

/*
 *  html_end_suppress - end the suppression of output.
 */
extern void html_end_suppress(int is_inline);

#endif
