# Target-dependent settings for FreeBSD/sparc64.
#  Copyright 2002 Free Software Foundation, Inc.
#  Contributed by David E. O'Brien <obrien@FreeBSD.org>.
#
#  This file is part of GDB.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

# Target: FreeBSD/sparc64
TDEPFILES= sparc-tdep.o solib.o solib-svr4.o solib-legacy.o 
TM_FILE= tm-fbsd.h
