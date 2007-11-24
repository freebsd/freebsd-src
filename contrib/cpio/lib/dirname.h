/*  Take file names apart into directory and base names.

    Copyright (C) 1998, 2001, 2003, 2004 Free Software Foundation, Inc.

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

#ifndef DIRNAME_H_
# define DIRNAME_H_ 1

# include <stdbool.h>
# include <stddef.h>

# ifndef DIRECTORY_SEPARATOR
#  define DIRECTORY_SEPARATOR '/'
# endif

# ifndef ISSLASH
#  define ISSLASH(C) ((C) == DIRECTORY_SEPARATOR)
# endif

# ifndef FILE_SYSTEM_PREFIX_LEN
#  define FILE_SYSTEM_PREFIX_LEN(Filename) 0
# endif

# define IS_ABSOLUTE_FILE_NAME(F) ISSLASH ((F)[FILE_SYSTEM_PREFIX_LEN (F)])
# define IS_RELATIVE_FILE_NAME(F) (! IS_ABSOLUTE_FILE_NAME (F))

char *base_name (char const *path);
char *dir_name (char const *path);
size_t base_len (char const *path);
size_t dir_len (char const *path);

bool strip_trailing_slashes (char *path);

#endif /* not DIRNAME_H_ */
