/* RCS map of character types */

/* Copyright (C) 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1995 by Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

#include "rcsbase.h"

libId(mapId, "$FreeBSD$")

/* map of character types */
/* ISO 8859/1 (Latin-1) */
enum tokens const ctab[] = {
	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,
	SPACE,	SPACE,	NEWLN,	SPACE,	SPACE,	SPACE,	UNKN,	UNKN,
	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,
	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,
	SPACE,	IDCHAR,	IDCHAR,	IDCHAR,	DELIM,	IDCHAR,	IDCHAR,	IDCHAR,
	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	DELIM,	IDCHAR,	PERIOD,	IDCHAR,
	DIGIT,	DIGIT,	DIGIT,	DIGIT,	DIGIT,	DIGIT,	DIGIT,	DIGIT,
	DIGIT,	DIGIT,	COLON,	SEMI,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,
	SBEGIN,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
	LETTER,	LETTER,	LETTER,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,
	IDCHAR,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,
	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,
	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,
	Letter,	Letter,	Letter,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	UNKN,
	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,
	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,
	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,
	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,	UNKN,
	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,
	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,
	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,
	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,	IDCHAR,
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	IDCHAR,
	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	LETTER,	Letter,
	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,
	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,
	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	IDCHAR,
	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter,	Letter
};
