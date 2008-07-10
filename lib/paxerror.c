/* Miscellaneous error functions

   Copyright (C) 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <system.h>
#include <paxlib.h>
#include <quote.h>
#include <quotearg.h>

/* Decode MODE from its binary form in a stat structure, and encode it
   into a 9-byte string STRING, terminated with a NUL.  */

void
pax_decode_mode (mode_t mode, char *string)
{
  *string++ = mode & S_IRUSR ? 'r' : '-';
  *string++ = mode & S_IWUSR ? 'w' : '-';
  *string++ = (mode & S_ISUID
	       ? (mode & S_IXUSR ? 's' : 'S')
	       : (mode & S_IXUSR ? 'x' : '-'));
  *string++ = mode & S_IRGRP ? 'r' : '-';
  *string++ = mode & S_IWGRP ? 'w' : '-';
  *string++ = (mode & S_ISGID
	       ? (mode & S_IXGRP ? 's' : 'S')
	       : (mode & S_IXGRP ? 'x' : '-'));
  *string++ = mode & S_IROTH ? 'r' : '-';
  *string++ = mode & S_IWOTH ? 'w' : '-';
  *string++ = (mode & S_ISVTX
	       ? (mode & S_IXOTH ? 't' : 'T')
	       : (mode & S_IXOTH ? 'x' : '-'));
  *string = '\0';
}

/* Report an error associated with the system call CALL and the
   optional name NAME.  */
void
call_arg_error (char const *call, char const *name)
{
  int e = errno;
  /* TRANSLATORS: %s after `Cannot' is a function name, e.g. `Cannot open'.
     Directly translating this to another language will not work, first because
     %s itself is not translated.
     Translate it as `%s: Function %s failed'. */
  ERROR ((0, e, _("%s: Cannot %s"), quotearg_colon (name), call));
}

/* Report a fatal error associated with the system call CALL and
   the optional file name NAME.  */
void
call_arg_fatal (char const *call, char const *name)
{
  int e = errno;
  /* TRANSLATORS: %s after `Cannot' is a function name, e.g. `Cannot open'.
     Directly translating this to another language will not work, first because
     %s itself is not translated.
     Translate it as `%s: Function %s failed'. */
  FATAL_ERROR ((0, e, _("%s: Cannot %s"), quotearg_colon (name),  call));
}

/* Report a warning associated with the system call CALL and
   the optional file name NAME.  */
void
call_arg_warn (char const *call, char const *name)
{
  int e = errno;
  /* TRANSLATORS: %s after `Cannot' is a function name, e.g. `Cannot open'.
     Directly translating this to another language will not work, first because
     %s itself is not translated.
     Translate it as `%s: Function %s failed'. */
  WARN ((0, e, _("%s: Warning: Cannot %s"), quotearg_colon (name), call));
}

void
chmod_error_details (char const *name, mode_t mode)
{
  int e = errno;
  char buf[10];
  pax_decode_mode (mode, buf);
  ERROR ((0, e, _("%s: Cannot change mode to %s"),
	  quotearg_colon (name), buf));
}

void
chown_error_details (char const *name, uid_t uid, gid_t gid)
{
  int e = errno;
  ERROR ((0, e, _("%s: Cannot change ownership to uid %lu, gid %lu"),
	  quotearg_colon (name), (unsigned long) uid, (unsigned long) gid));
}

void
close_error (char const *name)
{
  call_arg_error ("close", name);
}

void
close_warn (char const *name)
{
  call_arg_warn ("close", name);
}

void
exec_fatal (char const *name)
{
  call_arg_fatal ("exec", name);
}

void
link_error (char const *target, char const *source)
{
  int e = errno;
  ERROR ((0, e, _("%s: Cannot hard link to %s"),
	  quotearg_colon (source), quote_n (1, target)));
}

void
mkdir_error (char const *name)
{
  call_arg_error ("mkdir", name);
}

void
mkfifo_error (char const *name)
{
  call_arg_error ("mkfifo", name);
}

void
mknod_error (char const *name)
{
  call_arg_error ("mknod", name);
}

void
open_error (char const *name)
{
  call_arg_error ("open", name);
}

void
open_fatal (char const *name)
{
  call_arg_fatal ("open", name);
}

void
open_warn (char const *name)
{
  call_arg_warn ("open", name);
}

void
read_error (char const *name)
{
  call_arg_error ("read", name);
}

void
read_error_details (char const *name, off_t offset, size_t size)
{
  char buf[UINTMAX_STRSIZE_BOUND];
  int e = errno;
  ERROR ((0, e,
	  ngettext ("%s: Read error at byte %s, while reading %lu byte",
		    "%s: Read error at byte %s, while reading %lu bytes",
		    size),
	  quotearg_colon (name), STRINGIFY_BIGINT (offset, buf),
	  (unsigned long) size));
}

void
read_warn_details (char const *name, off_t offset, size_t size)
{
  char buf[UINTMAX_STRSIZE_BOUND];
  int e = errno;
  WARN ((0, e,
	 ngettext ("%s: Warning: Read error at byte %s, while reading %lu byte",
		   "%s: Warning: Read error at byte %s, while reading %lu bytes",
		   size),
	 quotearg_colon (name), STRINGIFY_BIGINT (offset, buf),
	 (unsigned long) size));
}

void
read_fatal (char const *name)
{
  call_arg_fatal ("read", name);
}

void
read_fatal_details (char const *name, off_t offset, size_t size)
{
  char buf[UINTMAX_STRSIZE_BOUND];
  int e = errno;
  FATAL_ERROR ((0, e,
		ngettext ("%s: Read error at byte %s, while reading %lu byte",
			  "%s: Read error at byte %s, while reading %lu bytes",
			  size),
		quotearg_colon (name), STRINGIFY_BIGINT (offset, buf),
		(unsigned long) size));
}

void
readlink_error (char const *name)
{
  call_arg_error ("readlink", name);
}

void
readlink_warn (char const *name)
{
  call_arg_warn ("readlink", name);
}

void
rmdir_error (char const *name)
{
  call_arg_error ("rmdir", name);
}

void
savedir_error (char const *name)
{
  call_arg_error ("savedir", name);
}

void
savedir_warn (char const *name)
{
  call_arg_warn ("savedir", name);
}

void
seek_error (char const *name)
{
  call_arg_error ("seek", name);
}

void
seek_error_details (char const *name, off_t offset)
{
  char buf[UINTMAX_STRSIZE_BOUND];
  int e = errno;
  ERROR ((0, e, _("%s: Cannot seek to %s"),
	  quotearg_colon (name),
	  STRINGIFY_BIGINT (offset, buf)));
}

void
seek_warn (char const *name)
{
  call_arg_warn ("seek", name);
}

void
seek_warn_details (char const *name, off_t offset)
{
  char buf[UINTMAX_STRSIZE_BOUND];
  int e = errno;
  WARN ((0, e, _("%s: Warning: Cannot seek to %s"),
	 quotearg_colon (name),
	 STRINGIFY_BIGINT (offset, buf)));
}

void
symlink_error (char const *contents, char const *name)
{
  int e = errno;
  ERROR ((0, e, _("%s: Cannot create symlink to %s"),
	  quotearg_colon (name), quote_n (1, contents)));
}

void
stat_fatal (char const *name)
{
  call_arg_fatal ("stat", name);
}

void
stat_error (char const *name)
{
  call_arg_error ("stat", name);
}

void
stat_warn (char const *name)
{
  call_arg_warn ("stat", name);
}

void
truncate_error (char const *name)
{
  call_arg_error ("truncate", name);
}

void
truncate_warn (char const *name)
{
  call_arg_warn ("truncate", name);
}

void
unlink_error (char const *name)
{
  call_arg_error ("unlink", name);
}

void
utime_error (char const *name)
{
  call_arg_error ("utime", name);
}

void
waitpid_error (char const *name)
{
  call_arg_error ("waitpid", name);
}

void
write_error (char const *name)
{
  call_arg_error ("write", name);
}

void
write_error_details (char const *name, size_t status, size_t size)
{
  if (status == 0)
    write_error (name);
  else
    ERROR ((0, 0,
	    ngettext ("%s: Wrote only %lu of %lu byte",
		      "%s: Wrote only %lu of %lu bytes",
		      size),
	    name, (unsigned long int) status, (unsigned long int) size));
}

void
write_fatal (char const *name)
{
  call_arg_fatal ("write", name);
}

void
chdir_fatal (char const *name)
{
  call_arg_fatal ("chdir", name);
}
