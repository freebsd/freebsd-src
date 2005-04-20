/* backupfile.h -- declarations for making Emacs style backup file names
   Copyright (C) 1990-1992, 1997-1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef BACKUPFILE_H_
# define BACKUPFILE_H_

/* When to make backup files. */
enum backup_type
{
  /* Never make backups. */
  none,

  /* Make simple backups of every file. */
  simple,

  /* Make numbered backups of files that already have numbered backups,
     and simple backups of the others. */
  numbered_existing,

  /* Make numbered backups of every file. */
  numbered
};

# define VALID_BACKUP_TYPE(Type)	\
  ((Type) == none			\
   || (Type) == simple			\
   || (Type) == numbered_existing	\
   || (Type) == numbered)

extern char const *simple_backup_suffix;

# ifndef PARAMS
#  if defined PROTOTYPES || (defined __STDC__ && __STDC__)
#   define PARAMS(Args) Args
#  else
#   define PARAMS(Args) ()
#  endif
# endif

char *find_backup_file_name PARAMS ((char const *, enum backup_type));
enum backup_type get_version PARAMS ((char const *context, char const *arg));
enum backup_type xget_version PARAMS ((char const *context, char const *arg));
void addext PARAMS ((char *, char const *, int));

#endif /* ! BACKUPFILE_H_ */
