/* lchown.c - dummy version of lchown for systems that do not store
   file owners of symbolic links.
   Copyright (C) 1995 Rodney W. Grimes, Accurate Automation Company

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Written by Rodney W. Grimes <rgrimes@FreeBSD.Org> */

#include <unistd.h>

int
lchown(const char *path, uid_t owner, gid_t group) {

	return (0);
}
