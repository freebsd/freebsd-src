/* Core dump and executable file functions above target vector, for GDB.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993, 1994
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdb_string.h"
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include "frame.h"  /* required by inferior.h */
#include "inferior.h"
#include "symtab.h"
#include "command.h"
#include "gdbcmd.h"
#include "bfd.h"
#include "target.h"
#include "gdbcore.h"
#include "dis-asm.h"
#include "language.h"
#include "gdb_stat.h"
#include "symfile.h"
#include "objfiles.h"

extern char registers[];

/* Local function declarations.  */

static void call_extra_exec_file_hooks PARAMS ((char *filename));

/* You can have any number of hooks for `exec_file_command' command to call.
   If there's only one hook, it is set in exec_file_display hook.
   If there are two or more hooks, they are set in exec_file_extra_hooks[],
   and exec_file_display_hook is set to a function that calls all of them.
   This extra complexity is needed to preserve compatibility with
   old code that assumed that only one hook could be set, and which called
   exec_file_display_hook directly.  */

typedef void (*hook_type) PARAMS ((char *));

hook_type exec_file_display_hook;		/* the original hook */
static hook_type *exec_file_extra_hooks;	/* array of additional hooks */
static int exec_file_hook_count = 0;		/* size of array */

/* Binary file diddling handle for the core file.  */

bfd *core_bfd = NULL;


/* Backward compatability with old way of specifying core files.  */

void
core_file_command (filename, from_tty)
     char *filename;
     int from_tty;
{
  struct target_ops *t;

  dont_repeat ();			/* Either way, seems bogus. */

  t = find_core_target ();
  if (t != NULL)
    if (!filename)
      (t->to_detach) (filename, from_tty);
    else
      {
        /* Yes, we were given the path of a core file.  Do we already
           have a symbol file?  If not, can we determine it from the
           core file?  If we can, do so.
           */
#ifdef HPUXHPPA
        if (symfile_objfile == NULL)
          {
            char *  symfile;
            symfile = t->to_core_file_to_sym_file (filename);
            if (symfile)
              {
                char *  symfile_copy = strdup (symfile);

                make_cleanup (free, symfile_copy);
                symbol_file_command (symfile_copy, from_tty);
              }
            else
              warning ("Unknown symbols for '%s'; use the 'symbol-file' command.", filename);
          }
#endif
        (t->to_open) (filename, from_tty);
      }
  else
    error ("GDB can't read core files on this machine.");
}


/* If there are two or more functions that wish to hook into exec_file_command,
 * this function will call all of the hook functions. */

static void
call_extra_exec_file_hooks (filename)
     char *filename;
{
  int i;

  for (i = 0; i < exec_file_hook_count; i++)
    (*exec_file_extra_hooks[i])(filename);
}

/* Call this to specify the hook for exec_file_command to call back.
   This is called from the x-window display code.  */

void
specify_exec_file_hook (hook)
     void (*hook) PARAMS ((char *));
{
  hook_type *new_array;

  if (exec_file_display_hook != NULL)
    {
      /* There's already a hook installed.  Arrange to have both it
       * and the subsequent hooks called. */
      if (exec_file_hook_count == 0)
	{
	  /* If this is the first extra hook, initialize the hook array. */
	  exec_file_extra_hooks = (hook_type *) xmalloc (sizeof(hook_type));
	  exec_file_extra_hooks[0] = exec_file_display_hook;
	  exec_file_display_hook = call_extra_exec_file_hooks;
	  exec_file_hook_count = 1;
	}

      /* Grow the hook array by one and add the new hook to the end.
         Yes, it's inefficient to grow it by one each time but since
         this is hardly ever called it's not a big deal.  */
      exec_file_hook_count++;
      new_array =
	(hook_type *) xrealloc (exec_file_extra_hooks,
				exec_file_hook_count * sizeof(hook_type));
      exec_file_extra_hooks = new_array;
      exec_file_extra_hooks[exec_file_hook_count - 1] = hook;
    }
  else
    exec_file_display_hook = hook;
}

/* The exec file must be closed before running an inferior.
   If it is needed again after the inferior dies, it must
   be reopened.  */

void
close_exec_file ()
{
#if 0 /* FIXME */
  if (exec_bfd)
    bfd_tempclose (exec_bfd);
#endif
}

void
reopen_exec_file ()
{
#if 0 /* FIXME */
  if (exec_bfd)
    bfd_reopen (exec_bfd);
#else
  char *filename;
  int res;
  struct stat st;
  long mtime;

  /* Don't do anything if the current target isn't exec. */
  if (exec_bfd == NULL || strcmp (target_shortname, "exec") != 0)
    return;
 
  /* If the timestamp of the exec file has changed, reopen it. */
  filename = strdup (bfd_get_filename (exec_bfd));
  make_cleanup (free, filename);
  mtime = bfd_get_mtime(exec_bfd);
  res = stat (filename, &st);

  if (mtime && mtime != st.st_mtime)
    exec_file_command (filename, 0);
#endif
}

/* If we have both a core file and an exec file,
   print a warning if they don't go together.  */

void
validate_files ()
{
  if (exec_bfd && core_bfd)
    {
      if (!core_file_matches_executable_p (core_bfd, exec_bfd))
	warning ("core file may not match specified executable file.");
      else if (bfd_get_mtime(exec_bfd) > bfd_get_mtime(core_bfd))
	warning ("exec file is newer than core file.");
    }
}

/* Return the name of the executable file as a string.
   ERR nonzero means get error if there is none specified;
   otherwise return 0 in that case.  */

char *
get_exec_file (err)
     int err;
{
  if (exec_bfd) return bfd_get_filename(exec_bfd);
  if (!err)     return NULL;

  error ("No executable file specified.\n\
Use the \"file\" or \"exec-file\" command.");
  return NULL;
}


/* Report a memory error with error().  */

void
memory_error (status, memaddr)
     int status;
     CORE_ADDR memaddr;
{
  if (status == EIO)
    {
      /* Actually, address between memaddr and memaddr + len
	 was out of bounds. */
      error_begin ();
      printf_filtered ("Cannot access memory at address ");
      print_address_numeric (memaddr, 1, gdb_stdout);
      printf_filtered (".\n");
      return_to_top_level (RETURN_ERROR);
    }
  else
    {
      error_begin ();
      printf_filtered ("Error accessing memory address ");
      print_address_numeric (memaddr, 1, gdb_stdout);
      printf_filtered (": %s.\n",
			 safe_strerror (status));
      return_to_top_level (RETURN_ERROR);
    }
}

/* Same as target_read_memory, but report an error if can't read.  */
void
read_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int status;
  status = target_read_memory (memaddr, myaddr, len);
  if (status != 0)
    memory_error (status, memaddr);
}

void
read_memory_section (memaddr, myaddr, len, bfd_section)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     asection *bfd_section;
{
  int status;
  status = target_read_memory_section (memaddr, myaddr, len, bfd_section);
  if (status != 0)
    memory_error (status, memaddr);
}

/* Like target_read_memory, but slightly different parameters.  */

int
dis_asm_read_memory (memaddr, myaddr, len, info)
     bfd_vma memaddr;
     bfd_byte *myaddr;
     int len;
     disassemble_info *info;
{
  return target_read_memory (memaddr, (char *) myaddr, len);
}

/* Like memory_error with slightly different parameters.  */
void
dis_asm_memory_error (status, memaddr, info)
     int status;
     bfd_vma memaddr;
     disassemble_info *info;
{
  memory_error (status, memaddr);
}

/* Like print_address with slightly different parameters.  */
void
dis_asm_print_address (addr, info)
     bfd_vma addr;
     struct disassemble_info *info;
{
  print_address (addr, info->stream);
}

/* Same as target_write_memory, but report an error if can't write.  */
void
write_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int status;

  status = target_write_memory (memaddr, myaddr, len);
  if (status != 0)
    memory_error (status, memaddr);
}

/* Read an integer from debugged memory, given address and number of bytes.  */

LONGEST
read_memory_integer (memaddr, len)
     CORE_ADDR memaddr;
     int len;
{
  char buf[sizeof (LONGEST)];

  read_memory (memaddr, buf, len);
  return extract_signed_integer (buf, len);
}

ULONGEST
read_memory_unsigned_integer (memaddr, len)
     CORE_ADDR memaddr;
     int len;
{
  char buf[sizeof (ULONGEST)];

  read_memory (memaddr, buf, len);
  return extract_unsigned_integer (buf, len);
}

void
read_memory_string (memaddr, buffer, max_len)
     CORE_ADDR memaddr;
     char * buffer;
     int max_len;
{
  register char * cp;
  register int i;
  int cnt;

  cp = buffer;
  while (1)
    {
      if (cp - buffer >= max_len)
        {
          buffer[max_len - 1] = '\0';
          break;
        }
      cnt = max_len - (cp - buffer);
      if (cnt > 8)
	cnt = 8;
      read_memory (memaddr + (int) (cp - buffer), cp, cnt);
      for (i = 0; i < cnt && *cp; i++, cp++)
        ; /* null body */

      if (i < cnt && !*cp)
        break;
    }
}


#if 0
/* Enable after 4.12.  It is not tested.  */

/* Search code.  Targets can just make this their search function, or
   if the protocol has a less general search function, they can call this
   in the cases it can't handle.  */
void
generic_search (len, data, mask, startaddr, increment, lorange, hirange
		addr_found, data_found)
     int len;
     char *data;
     char *mask;
     CORE_ADDR startaddr;
     int increment;
     CORE_ADDR lorange;
     CORE_ADDR hirange;
     CORE_ADDR *addr_found;
     char *data_found;
{
  int i;
  CORE_ADDR curaddr = startaddr;

  while (curaddr >= lorange && curaddr < hirange)
    {
      read_memory (curaddr, data_found, len);
      for (i = 0; i < len; ++i)
	if ((data_found[i] & mask[i]) != data[i])
	  goto try_again;
      /* It matches.  */
      *addr_found = curaddr;
      return;

    try_again:
      curaddr += increment;
    }
  *addr_found = (CORE_ADDR)0;
  return;
}
#endif /* 0 */

/* The current default bfd target.  Points to storage allocated for
   gnutarget_string.  */
char *gnutarget;

/* Same thing, except it is "auto" not NULL for the default case.  */
static char *gnutarget_string;

static void set_gnutarget_command
  PARAMS ((char *, int, struct cmd_list_element *));

static void
set_gnutarget_command (ignore, from_tty, c)
     char *ignore;
     int from_tty;
     struct cmd_list_element *c;
{
  if (STREQ (gnutarget_string, "auto"))
    gnutarget = NULL;
  else
    gnutarget = gnutarget_string;
}

/* Set the gnutarget.  */
void
set_gnutarget (newtarget)
     char *newtarget;
{
  if (gnutarget_string != NULL)
    free (gnutarget_string);
  gnutarget_string = savestring (newtarget, strlen (newtarget));
  set_gnutarget_command (NULL, 0, NULL);
}

void
_initialize_core()
{
  struct cmd_list_element *c;
  c = add_cmd ("core-file", class_files, core_file_command,
	       "Use FILE as core dump for examining memory and registers.\n\
No arg means have no core file.  This command has been superseded by the\n\
`target core' and `detach' commands.", &cmdlist);
  c->completer = filename_completer;

  c = add_set_cmd ("gnutarget", class_files, var_string_noescape,
		  (char *) &gnutarget_string,
		  "Set the current BFD target.\n\
Use `set gnutarget auto' to specify automatic detection.",
		  &setlist);
  c->function.sfunc = set_gnutarget_command;
  add_show_from_set (c, &showlist);

  if (getenv ("GNUTARGET"))
    set_gnutarget (getenv ("GNUTARGET"));
  else
    set_gnutarget ("auto");
}
