// -*- C++ -*-
/* Copyright (C) 2000 Free Software Foundation, Inc.
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

#if !defined(HTMLINDICATE_H)
#  define HTMLINDICATE_H

/*
 *  graphic_start - emit a html graphic start indicator, but only
 *                  if one has not already been issued.
 */

extern void graphic_start (void);


/*
 *  graphic_end - emit a html graphic end indicator, but only
 *                if a corresponding matching graphic-start has
 *                been issued.
 */

extern void graphic_end (void);

#endif

