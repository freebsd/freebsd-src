/* Select target systems and architectures at runtime for GDB.
   Copyright 1990, 1992-1995, 1998, 1999 Free Software Foundation, Inc.
   Contributed by Cygnus Support.

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
#include <errno.h>
#include <ctype.h>
#include "gdb_string.h"
#include "target.h"
#include "gdbcmd.h"
#include "symtab.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "wait.h"
#include <signal.h>

extern int errno;

static void
target_info PARAMS ((char *, int));

static void
cleanup_target PARAMS ((struct target_ops *));

static void
maybe_kill_then_create_inferior PARAMS ((char *, char *, char **));

static void
default_clone_and_follow_inferior PARAMS ((int, int *));

static void
maybe_kill_then_attach PARAMS ((char *, int));

static void
kill_or_be_killed PARAMS ((int));

static void
default_terminal_info PARAMS ((char *, int));

static int
nosymbol PARAMS ((char *, CORE_ADDR *));

static void
tcomplain PARAMS ((void));

static int
nomemory PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

static int
return_zero PARAMS ((void));

static int
return_one PARAMS ((void));

void
target_ignore PARAMS ((void));

static void
target_command PARAMS ((char *, int));

static struct target_ops *
find_default_run_target PARAMS ((char *));

static void
update_current_target PARAMS ((void));

/* Transfer LEN bytes between target address MEMADDR and GDB address MYADDR.
   Returns 0 for success, errno code for failure (which includes partial
   transfers--if you want a more useful response to partial transfers, try
   target_read_memory_partial).  */

static int
target_xfer_memory PARAMS ((CORE_ADDR memaddr, char *myaddr, int len,
			    int write, asection *bfd_section));

static void init_dummy_target PARAMS ((void));

static void
debug_to_open PARAMS ((char *, int));

static void
debug_to_close PARAMS ((int));

static void
debug_to_attach PARAMS ((char *, int));

static void
debug_to_detach PARAMS ((char *, int));

static void
debug_to_resume PARAMS ((int, int, enum target_signal));

static int
debug_to_wait PARAMS ((int, struct target_waitstatus *));

static void
debug_to_fetch_registers PARAMS ((int));

static void
debug_to_store_registers PARAMS ((int));

static void
debug_to_prepare_to_store PARAMS ((void));

static int
debug_to_xfer_memory PARAMS ((CORE_ADDR, char *, int, int, struct target_ops *));

static void
debug_to_files_info PARAMS ((struct target_ops *));

static int
debug_to_insert_breakpoint PARAMS ((CORE_ADDR, char *));

static int
debug_to_remove_breakpoint PARAMS ((CORE_ADDR, char *));

static void
debug_to_terminal_init PARAMS ((void));

static void
debug_to_terminal_inferior PARAMS ((void));

static void
debug_to_terminal_ours_for_output PARAMS ((void));

static void
debug_to_terminal_ours PARAMS ((void));

static void
debug_to_terminal_info PARAMS ((char *, int));

static void
debug_to_kill PARAMS ((void));

static void
debug_to_load PARAMS ((char *, int));

static int
debug_to_lookup_symbol PARAMS ((char *, CORE_ADDR *));

static void
debug_to_create_inferior PARAMS ((char *, char *, char **));

static void
debug_to_mourn_inferior PARAMS ((void));

static int
debug_to_can_run PARAMS ((void));

static void
debug_to_notice_signals PARAMS ((int));

static int
debug_to_thread_alive PARAMS ((int));

static void
debug_to_stop PARAMS ((void));

static int debug_to_query PARAMS ((int/*char*/, char *, char *, int *));

/* Pointer to array of target architecture structures; the size of the
   array; the current index into the array; the allocated size of the 
   array.  */
struct target_ops **target_structs;
unsigned target_struct_size;
unsigned target_struct_index;
unsigned target_struct_allocsize;
#define	DEFAULT_ALLOCSIZE	10

/* The initial current target, so that there is always a semi-valid
   current target.  */

static struct target_ops dummy_target;

/* Top of target stack.  */

struct target_stack_item *target_stack;

/* The target structure we are currently using to talk to a process
   or file or whatever "inferior" we have.  */

struct target_ops current_target;

/* Command list for target.  */

static struct cmd_list_element *targetlist = NULL;

/* Nonzero if we are debugging an attached outside process
   rather than an inferior.  */

int attach_flag;

#ifdef MAINTENANCE_CMDS
/* Non-zero if we want to see trace of target level stuff.  */

static int targetdebug = 0;

static void setup_target_debug PARAMS ((void));

#endif

/* The user just typed 'target' without the name of a target.  */

/* ARGSUSED */
static void
target_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  fputs_filtered ("Argument required (target name).  Try `help target'\n",
		  gdb_stdout);
}

/* Add a possible target architecture to the list.  */

void
add_target (t)
     struct target_ops *t;
{
  if (!target_structs)
    {
      target_struct_allocsize = DEFAULT_ALLOCSIZE;
      target_structs = (struct target_ops **) xmalloc
	(target_struct_allocsize * sizeof (*target_structs));
    }
  if (target_struct_size >= target_struct_allocsize)
    {
      target_struct_allocsize *= 2;
      target_structs = (struct target_ops **)
	  xrealloc ((char *) target_structs, 
		    target_struct_allocsize * sizeof (*target_structs));
    }
  target_structs[target_struct_size++] = t;
/*  cleanup_target (t);*/

  if (targetlist == NULL)
    add_prefix_cmd ("target", class_run, target_command,
		    "Connect to a target machine or process.\n\
The first argument is the type or protocol of the target machine.\n\
Remaining arguments are interpreted by the target protocol.  For more\n\
information on the arguments for a particular protocol, type\n\
`help target ' followed by the protocol name.",
		    &targetlist, "target ", 0, &cmdlist);
  add_cmd (t->to_shortname, no_class, t->to_open, t->to_doc, &targetlist);
}

/* Stub functions */

void
target_ignore ()
{
}

/* ARGSUSED */
static int
nomemory (memaddr, myaddr, len, write, t)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *t;
{
  errno = EIO;		/* Can't read/write this location */
  return 0;		/* No bytes handled */
}

static void
tcomplain ()
{
  error ("You can't do that when your target is `%s'",
	 current_target.to_shortname);
}

void
noprocess ()
{
  error ("You can't do that without a process to debug.");
}

/* ARGSUSED */
static int
nosymbol (name, addrp)
     char *name;
     CORE_ADDR *addrp;
{
  return 1;		/* Symbol does not exist in target env */
}

/* ARGSUSED */
void
nosupport_runtime ()
{
  if (!inferior_pid)
    noprocess ();
  else
    error ("No run-time support for this");
}


/* ARGSUSED */
static void
default_terminal_info (args, from_tty)
     char *args;
     int from_tty;
{
  printf_unfiltered("No saved terminal information.\n");
}

/* This is the default target_create_inferior and target_attach function.
   If the current target is executing, it asks whether to kill it off.
   If this function returns without calling error(), it has killed off
   the target, and the operation should be attempted.  */

static void
kill_or_be_killed (from_tty)
     int from_tty;
{
  if (target_has_execution)
    {
      printf_unfiltered ("You are already running a program:\n");
      target_files_info ();
      if (query ("Kill it? ")) {
	target_kill ();
	if (target_has_execution)
	  error ("Killing the program did not help.");
	return;
      } else {
	error ("Program not killed.");
      }
    }
  tcomplain();
}

static void
maybe_kill_then_attach (args, from_tty)
     char *args;
     int from_tty;
{
  kill_or_be_killed (from_tty);
  target_attach (args, from_tty);
}

static void
maybe_kill_then_create_inferior (exec, args, env)
     char *exec;
     char *args;
     char **env;
{
  kill_or_be_killed (0);
  target_create_inferior (exec, args, env);
}

static void
default_clone_and_follow_inferior (child_pid, followed_child)
  int  child_pid;
  int  *followed_child;
{
  target_clone_and_follow_inferior (child_pid, followed_child);
}

/* Clean up a target struct so it no longer has any zero pointers in it.
   We default entries, at least to stubs that print error messages.  */

static void
cleanup_target (t)
     struct target_ops *t;
{

#define de_fault(field, value) \
  if (!t->field)	t->field = value

  /*        FIELD			DEFAULT VALUE        */

  de_fault (to_open, 			(void (*) PARAMS((char *, int))) tcomplain);
  de_fault (to_close, 			(void (*) PARAMS((int))) target_ignore);
  de_fault (to_attach, 			maybe_kill_then_attach);
  de_fault (to_post_attach,             (void (*) PARAMS ((int))) target_ignore);
  de_fault (to_require_attach,          maybe_kill_then_attach);
  de_fault (to_detach, 			(void (*) PARAMS((char *, int))) target_ignore);
  de_fault (to_require_detach,          (void (*) PARAMS((int, char *, int))) target_ignore);
  de_fault (to_resume, 			(void (*) PARAMS((int, int, enum target_signal))) noprocess);
  de_fault (to_wait, 			(int (*) PARAMS((int, struct target_waitstatus *))) noprocess);
  de_fault (to_post_wait,               (void (*) PARAMS ((int, int))) target_ignore);
  de_fault (to_fetch_registers, 	(void (*) PARAMS((int))) target_ignore);
  de_fault (to_store_registers,		(void (*) PARAMS((int))) noprocess);
  de_fault (to_prepare_to_store,	(void (*) PARAMS((void))) noprocess);
  de_fault (to_xfer_memory,		(int (*) PARAMS((CORE_ADDR, char *, int, int, struct target_ops *))) nomemory);
  de_fault (to_files_info,		(void (*) PARAMS((struct target_ops *))) target_ignore);
  de_fault (to_insert_breakpoint,	memory_insert_breakpoint);
  de_fault (to_remove_breakpoint,	memory_remove_breakpoint);
  de_fault (to_terminal_init,		(void (*) PARAMS((void))) target_ignore);
  de_fault (to_terminal_inferior,	(void (*) PARAMS ((void))) target_ignore);
  de_fault (to_terminal_ours_for_output,(void (*) PARAMS ((void))) target_ignore);
  de_fault (to_terminal_ours,		(void (*) PARAMS ((void))) target_ignore);
  de_fault (to_terminal_info,		default_terminal_info);
  de_fault (to_kill,			(void (*) PARAMS((void))) noprocess);
  de_fault (to_load,			(void (*) PARAMS((char *, int))) tcomplain);
  de_fault (to_lookup_symbol,		(int (*) PARAMS ((char *, CORE_ADDR *))) nosymbol);
  de_fault (to_create_inferior,		maybe_kill_then_create_inferior);
  de_fault (to_post_startup_inferior,   (void (*) PARAMS ((int))) target_ignore);
  de_fault (to_acknowledge_created_inferior,            (void (*) PARAMS((int))) target_ignore);
  de_fault (to_clone_and_follow_inferior,               default_clone_and_follow_inferior);
  de_fault (to_post_follow_inferior_by_clone,           (void (*) PARAMS ((void))) target_ignore);
  de_fault (to_insert_fork_catchpoint,  (int (*) PARAMS ((int))) tcomplain);
  de_fault (to_remove_fork_catchpoint,  (int (*) PARAMS ((int))) tcomplain);
  de_fault (to_insert_vfork_catchpoint, (int (*) PARAMS ((int))) tcomplain);
  de_fault (to_remove_vfork_catchpoint, (int (*) PARAMS ((int))) tcomplain);
  de_fault (to_has_forked,              (int (*) PARAMS ((int, int *))) return_zero);
  de_fault (to_has_vforked,             (int (*) PARAMS ((int, int *))) return_zero);
  de_fault (to_can_follow_vfork_prior_to_exec, (int (*) PARAMS ((void ))) return_zero);
  de_fault (to_post_follow_vfork,       (void (*) PARAMS ((int, int, int, int))) target_ignore);
  de_fault (to_insert_exec_catchpoint,  (int (*) PARAMS ((int))) tcomplain);
  de_fault (to_remove_exec_catchpoint,  (int (*) PARAMS ((int))) tcomplain);
  de_fault (to_has_execd,               (int (*) PARAMS ((int, char **))) return_zero);
  de_fault (to_reported_exec_events_per_exec_call, (int (*) PARAMS ((void))) return_one);
  de_fault (to_has_syscall_event,       (int (*) PARAMS ((int, enum target_waitkind *, int *))) return_zero);
  de_fault (to_has_exited,              (int (*) PARAMS ((int, int, int *))) return_zero);
  de_fault (to_mourn_inferior,		(void (*) PARAMS ((void))) noprocess);
  de_fault (to_can_run,			return_zero);
  de_fault (to_notice_signals,		(void (*) PARAMS((int))) target_ignore);
  de_fault (to_thread_alive,		(int (*) PARAMS((int))) target_ignore);
  de_fault (to_stop,			(void (*) PARAMS((void))) target_ignore);
  de_fault (to_query,			(int (*) PARAMS((int/*char*/, char*, char *, int *))) target_ignore);
  de_fault (to_enable_exception_callback,	(struct symtab_and_line * (*) PARAMS((enum exception_event_kind, int))) nosupport_runtime);
  de_fault (to_get_current_exception_event,	(struct exception_event_record * (*) PARAMS((void))) nosupport_runtime);

  de_fault (to_pid_to_exec_file,        (char* (*) PARAMS((int))) return_zero);
  de_fault (to_core_file_to_sym_file,   (char* (*) PARAMS ((char *))) return_zero);
#undef de_fault
}

/* Go through the target stack from top to bottom, copying over zero entries in
   current_target.  In effect, we are doing class inheritance through the
   pushed target vectors.  */

static void
update_current_target ()
{
  struct target_stack_item *item;
  struct target_ops *t;

  /* First, reset current_target */
  memset (&current_target, 0, sizeof current_target);

  for (item = target_stack; item; item = item->next)
    {
      t = item->target_ops;

#define INHERIT(FIELD, TARGET) \
      if (!current_target.FIELD) \
	current_target.FIELD = TARGET->FIELD

      INHERIT (to_shortname, t);
      INHERIT (to_longname, t);
      INHERIT (to_doc, t);
      INHERIT (to_open, t);
      INHERIT (to_close, t);
      INHERIT (to_attach, t);
      INHERIT (to_post_attach, t);
      INHERIT (to_require_attach, t);
      INHERIT (to_detach, t);
      INHERIT (to_require_detach, t);
      INHERIT (to_resume, t);
      INHERIT (to_wait, t);
      INHERIT (to_post_wait, t);
      INHERIT (to_fetch_registers, t);
      INHERIT (to_store_registers, t);
      INHERIT (to_prepare_to_store, t);
      INHERIT (to_xfer_memory, t);
      INHERIT (to_files_info, t);
      INHERIT (to_insert_breakpoint, t);
      INHERIT (to_remove_breakpoint, t);
      INHERIT (to_terminal_init, t);
      INHERIT (to_terminal_inferior, t);
      INHERIT (to_terminal_ours_for_output, t);
      INHERIT (to_terminal_ours, t);
      INHERIT (to_terminal_info, t);
      INHERIT (to_kill, t);
      INHERIT (to_load, t);
      INHERIT (to_lookup_symbol, t);
      INHERIT (to_create_inferior, t);
      INHERIT (to_post_startup_inferior, t);
      INHERIT (to_acknowledge_created_inferior, t);
      INHERIT (to_clone_and_follow_inferior, t);
      INHERIT (to_post_follow_inferior_by_clone, t);
      INHERIT (to_insert_fork_catchpoint, t);
      INHERIT (to_remove_fork_catchpoint, t);
      INHERIT (to_insert_vfork_catchpoint, t);
      INHERIT (to_remove_vfork_catchpoint, t);
      INHERIT (to_has_forked, t);
      INHERIT (to_has_vforked, t);
      INHERIT (to_can_follow_vfork_prior_to_exec, t);
      INHERIT (to_post_follow_vfork, t);
      INHERIT (to_insert_exec_catchpoint, t);
      INHERIT (to_remove_exec_catchpoint, t);
      INHERIT (to_has_execd, t);
      INHERIT (to_reported_exec_events_per_exec_call, t);
      INHERIT (to_has_syscall_event, t);
      INHERIT (to_has_exited, t);
      INHERIT (to_mourn_inferior, t);
      INHERIT (to_can_run, t);
      INHERIT (to_notice_signals, t);
      INHERIT (to_thread_alive, t);
      INHERIT (to_stop, t);
      INHERIT (to_query, t);
      INHERIT (to_enable_exception_callback, t);
      INHERIT (to_get_current_exception_event, t);
      INHERIT (to_pid_to_exec_file, t);
      INHERIT (to_core_file_to_sym_file, t);
      INHERIT (to_stratum, t);
      INHERIT (DONT_USE, t);
      INHERIT (to_has_all_memory, t);
      INHERIT (to_has_memory, t);
      INHERIT (to_has_stack, t);
      INHERIT (to_has_registers, t);
      INHERIT (to_has_execution, t);
      INHERIT (to_has_thread_control, t);
      INHERIT (to_sections, t);
      INHERIT (to_sections_end, t);
      INHERIT (to_magic, t);

#undef INHERIT
    }
}

/* Push a new target type into the stack of the existing target accessors,
   possibly superseding some of the existing accessors.

   Result is zero if the pushed target ended up on top of the stack,
   nonzero if at least one target is on top of it.

   Rather than allow an empty stack, we always have the dummy target at
   the bottom stratum, so we can call the function vectors without
   checking them.  */

int
push_target (t)
     struct target_ops *t;
{
  struct target_stack_item *cur, *prev, *tmp;

  /* Check magic number.  If wrong, it probably means someone changed
     the struct definition, but not all the places that initialize one.  */
  if (t->to_magic != OPS_MAGIC)
    {
      fprintf_unfiltered(gdb_stderr,
			 "Magic number of %s target struct wrong\n", 
			 t->to_shortname);
      abort();
    }

  /* Find the proper stratum to install this target in. */

  for (prev = NULL, cur = target_stack; cur; prev = cur, cur = cur->next)
    {
      if ((int)(t->to_stratum) >= (int)(cur->target_ops->to_stratum))
	break;
    }

  /* If there's already targets at this stratum, remove them. */

  if (cur)
    while (t->to_stratum == cur->target_ops->to_stratum)
      {
	/* There's already something on this stratum.  Close it off.  */
	if (cur->target_ops->to_close)
	  (cur->target_ops->to_close) (0);
	if (prev)
	  prev->next = cur->next; /* Unchain old target_ops */
	else
	  target_stack = cur->next; /* Unchain first on list */
	tmp = cur->next;
	free (cur);
	cur = tmp;
      }

  /* We have removed all targets in our stratum, now add the new one.  */

  tmp = (struct target_stack_item *)
    xmalloc (sizeof (struct target_stack_item));
  tmp->next = cur;
  tmp->target_ops = t;

  if (prev)
    prev->next = tmp;
  else
    target_stack = tmp;

  update_current_target ();

  cleanup_target (&current_target); /* Fill in the gaps */

#ifdef MAINTENANCE_CMDS
  if (targetdebug)
    setup_target_debug ();
#endif

  return prev != 0;
}

/* Remove a target_ops vector from the stack, wherever it may be. 
   Return how many times it was removed (0 or 1).  */

int
unpush_target (t)
     struct target_ops *t;
{
  struct target_stack_item *cur, *prev;

  if (t->to_close)
    t->to_close (0);		/* Let it clean up */

  /* Look for the specified target.  Note that we assume that a target
     can only occur once in the target stack. */

  for (cur = target_stack, prev = NULL; cur; prev = cur, cur = cur->next)
    if (cur->target_ops == t)
      break;

  if (!cur)
    return 0;			/* Didn't find target_ops, quit now */

  /* Unchain the target */

  if (!prev)
    target_stack = cur->next;
  else
    prev->next = cur->next;

  free (cur);			/* Release the target_stack_item */

  update_current_target ();
  cleanup_target (&current_target);

  return 1;
}

void
pop_target ()
{
  (current_target.to_close)(0);	/* Let it clean up */
  if (unpush_target (target_stack->target_ops) == 1)
    return;

  fprintf_unfiltered(gdb_stderr,
		     "pop_target couldn't find target %s\n", 
		     current_target.to_shortname);
  abort();
}

#undef	MIN
#define MIN(A, B) (((A) <= (B)) ? (A) : (B))

/* target_read_string -- read a null terminated string, up to LEN bytes,
   from MEMADDR in target.  Set *ERRNOP to the errno code, or 0 if successful.
   Set *STRING to a pointer to malloc'd memory containing the data; the caller
   is responsible for freeing it.  Return the number of bytes successfully
   read.  */

int
target_read_string (memaddr, string, len, errnop)
     CORE_ADDR memaddr;
     char **string;
     int len;
     int *errnop;
{
  int tlen, origlen, offset, i;
  char buf[4];
  int errcode = 0;
  char *buffer;
  int buffer_allocated;
  char *bufptr;
  unsigned int nbytes_read = 0;

  /* Small for testing.  */
  buffer_allocated = 4;
  buffer = xmalloc (buffer_allocated);
  bufptr = buffer;

  origlen = len;

  while (len > 0)
    {
      tlen = MIN (len, 4 - (memaddr & 3));
      offset = memaddr & 3;

      errcode = target_xfer_memory (memaddr & ~3, buf, 4, 0, NULL);
      if (errcode != 0)
	{
	  /* The transfer request might have crossed the boundary to an
	     unallocated region of memory. Retry the transfer, requesting
	     a single byte.  */
	  tlen = 1;
	  offset = 0;
	  errcode = target_xfer_memory (memaddr, buf, 1, 0, NULL);
	  if (errcode != 0)
	    goto done;
	}

      if (bufptr - buffer + tlen > buffer_allocated)
	{
	  unsigned int bytes;
	  bytes = bufptr - buffer;
	  buffer_allocated *= 2;
	  buffer = xrealloc (buffer, buffer_allocated);
	  bufptr = buffer + bytes;
	}

      for (i = 0; i < tlen; i++)
	{
	  *bufptr++ = buf[i + offset];
	  if (buf[i + offset] == '\000')
	    {
	      nbytes_read += i + 1;
	      goto done;
	    }
	}

      memaddr += tlen;
      len -= tlen;
      nbytes_read += tlen;
    }
 done:
  if (errnop != NULL)
    *errnop = errcode;
  if (string != NULL)
    *string = buffer;
  return nbytes_read;
}

/* Read LEN bytes of target memory at address MEMADDR, placing the results in
   GDB's memory at MYADDR.  Returns either 0 for success or an errno value
   if any error occurs.

   If an error occurs, no guarantee is made about the contents of the data at
   MYADDR.  In particular, the caller should not depend upon partial reads
   filling the buffer with good data.  There is no way for the caller to know
   how much good data might have been transfered anyway.  Callers that can
   deal with partial reads should call target_read_memory_partial. */

int
target_read_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  return target_xfer_memory (memaddr, myaddr, len, 0, NULL);
}

int
target_read_memory_section (memaddr, myaddr, len, bfd_section)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     asection *bfd_section;
{
  return target_xfer_memory (memaddr, myaddr, len, 0, bfd_section);
}

/* Read LEN bytes of target memory at address MEMADDR, placing the results
   in GDB's memory at MYADDR.  Returns a count of the bytes actually read,
   and optionally an errno value in the location pointed to by ERRNOPTR
   if ERRNOPTR is non-null. */

int
target_read_memory_partial (memaddr, myaddr, len, errnoptr)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int *errnoptr;
{
  int nread;	/* Number of bytes actually read. */
  int errcode;	/* Error from last read. */

  /* First try a complete read. */
  errcode = target_xfer_memory (memaddr, myaddr, len, 0, NULL);
  if (errcode == 0)
    {
      /* Got it all. */
      nread = len;
    }
  else
    {
      /* Loop, reading one byte at a time until we get as much as we can. */
      for (errcode = 0, nread = 0; len > 0 && errcode == 0; nread++, len--)
	{
	  errcode = target_xfer_memory (memaddr++, myaddr++, 1, 0, NULL);
	}
      /* If an error, the last read was unsuccessful, so adjust count. */
      if (errcode != 0)
	{
	  nread--;
	}
    }
  if (errnoptr != NULL)
    {
      *errnoptr = errcode;
    }
  return (nread);
}

int
target_write_memory (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  return target_xfer_memory (memaddr, myaddr, len, 1, NULL);
}
 
/* This variable is used to pass section information down to targets.  This
   *should* be done by adding an argument to the target_xfer_memory function
   of all the targets, but I didn't feel like changing 50+ files.  */

asection *target_memory_bfd_section = NULL;

/* Move memory to or from the targets.  Iterate until all of it has
   been moved, if necessary.  The top target gets priority; anything
   it doesn't want, is offered to the next one down, etc.  Note the
   business with curlen:  if an early target says "no, but I have a
   boundary overlapping this xfer" then we shorten what we offer to
   the subsequent targets so the early guy will get a chance at the
   tail before the subsequent ones do. 

   Result is 0 or errno value.  */

static int
target_xfer_memory (memaddr, myaddr, len, write, bfd_section)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     asection *bfd_section;
{
  int curlen;
  int res;
  struct target_ops *t;
  struct target_stack_item *item;

  /* Zero length requests are ok and require no work.  */
  if (len == 0)
    return 0;

  target_memory_bfd_section = bfd_section;

  /* to_xfer_memory is not guaranteed to set errno, even when it returns
     0.  */
  errno = 0;

  /* The quick case is that the top target does it all.  */
  res = current_target.to_xfer_memory
			(memaddr, myaddr, len, write, &current_target);
  if (res == len)
    return 0;

  if (res > 0)
    goto bump;
  /* If res <= 0 then we call it again in the loop.  Ah well.  */

  for (; len > 0;)
    {
      curlen = len;		/* Want to do it all */
      for (item = target_stack; item; item = item->next)
	{
	  t = item->target_ops;
	  if (!t->to_has_memory)
	    continue;

	  res = t->to_xfer_memory (memaddr, myaddr, curlen, write, t);
	  if (res > 0)
	    break;		/* Handled all or part of xfer */
	  if (t->to_has_all_memory)
	    break;
	}

      if (res <= 0)
	{
	  /* If this address is for nonexistent memory,
	     read zeros if reading, or do nothing if writing.  Return error. */
	  if (!write)
	    memset (myaddr, 0, len);
	  if (errno == 0)
	    return EIO;
	  else
	    return errno;
	}
bump:
      memaddr += res;
      myaddr  += res;
      len     -= res;
    }
  return 0;			/* We managed to cover it all somehow. */
}


/* ARGSUSED */
static void
target_info (args, from_tty)
     char *args;
     int from_tty;
{
  struct target_ops *t;
  struct target_stack_item *item;
  int has_all_mem = 0;
  
  if (symfile_objfile != NULL)
    printf_unfiltered ("Symbols from \"%s\".\n", symfile_objfile->name);

#ifdef FILES_INFO_HOOK
  if (FILES_INFO_HOOK ())
    return;
#endif

  for (item = target_stack; item; item = item->next)
    {
      t = item->target_ops;

      if (!t->to_has_memory)
	continue;

      if ((int)(t->to_stratum) <= (int)dummy_stratum)
	continue;
      if (has_all_mem)
	printf_unfiltered("\tWhile running this, GDB does not access memory from...\n");
      printf_unfiltered("%s:\n", t->to_longname);
      (t->to_files_info)(t);
      has_all_mem = t->to_has_all_memory;
    }
}

/* This is to be called by the open routine before it does
   anything.  */

void
target_preopen (from_tty)
     int from_tty;
{
  dont_repeat();

  if (target_has_execution)
    {   
      if (query ("A program is being debugged already.  Kill it? "))
        target_kill ();
      else
        error ("Program not killed.");
    }

  /* Calling target_kill may remove the target from the stack.  But if
     it doesn't (which seems like a win for UDI), remove it now.  */

  if (target_has_execution)
    pop_target ();
}

/* Detach a target after doing deferred register stores.  */

void
target_detach (args, from_tty)
     char *args;
     int from_tty;
{
  /* Handle any optimized stores to the inferior.  */
#ifdef DO_DEFERRED_STORES
  DO_DEFERRED_STORES;
#endif
  (current_target.to_detach) (args, from_tty);
}

void
target_link (modname, t_reloc)
     char *modname;
     CORE_ADDR *t_reloc;
{
  if (STREQ(current_target.to_shortname, "rombug"))
    {
      (current_target.to_lookup_symbol) (modname, t_reloc);
      if (*t_reloc == 0)
      error("Unable to link to %s and get relocation in rombug", modname);
    }
  else
    *t_reloc = (CORE_ADDR)-1;
}

/* Look through the list of possible targets for a target that can
   execute a run or attach command without any other data.  This is
   used to locate the default process stratum.

   Result is always valid (error() is called for errors).  */

static struct target_ops *
find_default_run_target (do_mesg)
     char *do_mesg;
{
  struct target_ops **t;
  struct target_ops *runable = NULL;
  int count;

  count = 0;

  for (t = target_structs; t < target_structs + target_struct_size;
       ++t)
    {
      if ((*t)->to_can_run && target_can_run(*t))
	{
	  runable = *t;
	  ++count;
	}
    }

  if (count != 1)
    error ("Don't know how to %s.  Try \"help target\".", do_mesg);

  return runable;
}

void
find_default_attach (args, from_tty)
     char *args;
     int from_tty;
{
  struct target_ops *t;

  t = find_default_run_target("attach");
  (t->to_attach) (args, from_tty);
  return;
}

void
find_default_require_attach (args, from_tty)
     char *args;
     int from_tty;
{
  struct target_ops *t;

  t = find_default_run_target("require_attach");
  (t->to_require_attach) (args, from_tty);
  return;
}

void
find_default_require_detach (pid, args, from_tty)
  int  pid;
  char *  args;
  int  from_tty;
{
  struct target_ops *t;

  t = find_default_run_target("require_detach");
  (t->to_require_detach) (pid, args, from_tty);
  return;
}

void
find_default_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  struct target_ops *t;

  t = find_default_run_target("run");
  (t->to_create_inferior) (exec_file, allargs, env);
  return;
}

void
find_default_clone_and_follow_inferior (child_pid, followed_child)
  int  child_pid;
  int  *followed_child;
{
  struct target_ops *t;

  t = find_default_run_target("run");
  (t->to_clone_and_follow_inferior) (child_pid, followed_child);
  return;
}

static int
return_zero ()
{
  return 0;
}

static int
return_one ()
{
  return 1;
}

struct target_ops *
find_core_target ()
{
  struct target_ops **t;
  struct target_ops *runable = NULL;
  int count;
  
  count = 0;
  
  for (t = target_structs; t < target_structs + target_struct_size;
       ++t)
    {
      if ((*t)->to_stratum == (kernel_debugging ? kcore_stratum : core_stratum))
	{
	  runable = *t;
	  ++count;
	}
    }
  
  return(count == 1 ? runable : NULL);
}

/* The inferior process has died.  Long live the inferior!  */

void
generic_mourn_inferior ()
{
  extern int show_breakpoint_hit_counts;

  inferior_pid = 0;
  attach_flag = 0;
  breakpoint_init_inferior (inf_exited);
  registers_changed ();

#ifdef CLEAR_DEFERRED_STORES
  /* Delete any pending stores to the inferior... */
  CLEAR_DEFERRED_STORES;
#endif

  reopen_exec_file ();
  reinit_frame_cache ();

  /* It is confusing to the user for ignore counts to stick around
     from previous runs of the inferior.  So clear them.  */
  /* However, it is more confusing for the ignore counts to disappear when
     using hit counts.  So don't clear them if we're counting hits.  */
  if (!show_breakpoint_hit_counts)
    breakpoint_clear_ignore_counts ();
}

/* This table must match in order and size the signals in enum target_signal
   in target.h.  */
static struct {
  char *name;
  char *string;
  } signals [] =
{
  {"0", "Signal 0"},
  {"SIGHUP", "Hangup"},
  {"SIGINT", "Interrupt"},
  {"SIGQUIT", "Quit"},
  {"SIGILL", "Illegal instruction"},
  {"SIGTRAP", "Trace/breakpoint trap"},
  {"SIGABRT", "Aborted"},
  {"SIGEMT", "Emulation trap"},
  {"SIGFPE", "Arithmetic exception"},
  {"SIGKILL", "Killed"},
  {"SIGBUS", "Bus error"},
  {"SIGSEGV", "Segmentation fault"},
  {"SIGSYS", "Bad system call"},
  {"SIGPIPE", "Broken pipe"},
  {"SIGALRM", "Alarm clock"},
  {"SIGTERM", "Terminated"},
  {"SIGURG", "Urgent I/O condition"},
  {"SIGSTOP", "Stopped (signal)"},
  {"SIGTSTP", "Stopped (user)"},
  {"SIGCONT", "Continued"},
  {"SIGCHLD", "Child status changed"},
  {"SIGTTIN", "Stopped (tty input)"},
  {"SIGTTOU", "Stopped (tty output)"},
  {"SIGIO", "I/O possible"},
  {"SIGXCPU", "CPU time limit exceeded"},
  {"SIGXFSZ", "File size limit exceeded"},
  {"SIGVTALRM", "Virtual timer expired"},
  {"SIGPROF", "Profiling timer expired"},
  {"SIGWINCH", "Window size changed"},
  {"SIGLOST", "Resource lost"},
  {"SIGUSR1", "User defined signal 1"},
  {"SIGUSR2", "User defined signal 2"},
  {"SIGPWR", "Power fail/restart"},
  {"SIGPOLL", "Pollable event occurred"},
  {"SIGWIND", "SIGWIND"},
  {"SIGPHONE", "SIGPHONE"},
  {"SIGWAITING", "Process's LWPs are blocked"},
  {"SIGLWP", "Signal LWP"},
  {"SIGDANGER", "Swap space dangerously low"},
  {"SIGGRANT", "Monitor mode granted"},
  {"SIGRETRACT", "Need to relinquish monitor mode"},
  {"SIGMSG", "Monitor mode data available"},
  {"SIGSOUND", "Sound completed"},
  {"SIGSAK", "Secure attention"},
  {"SIGPRIO", "SIGPRIO"},
  {"SIG33", "Real-time event 33"},
  {"SIG34", "Real-time event 34"},
  {"SIG35", "Real-time event 35"},
  {"SIG36", "Real-time event 36"},
  {"SIG37", "Real-time event 37"},
  {"SIG38", "Real-time event 38"},
  {"SIG39", "Real-time event 39"},
  {"SIG40", "Real-time event 40"},
  {"SIG41", "Real-time event 41"},
  {"SIG42", "Real-time event 42"},
  {"SIG43", "Real-time event 43"},
  {"SIG44", "Real-time event 44"},
  {"SIG45", "Real-time event 45"},
  {"SIG46", "Real-time event 46"},
  {"SIG47", "Real-time event 47"},
  {"SIG48", "Real-time event 48"},
  {"SIG49", "Real-time event 49"},
  {"SIG50", "Real-time event 50"},
  {"SIG51", "Real-time event 51"},
  {"SIG52", "Real-time event 52"},
  {"SIG53", "Real-time event 53"},
  {"SIG54", "Real-time event 54"},
  {"SIG55", "Real-time event 55"},
  {"SIG56", "Real-time event 56"},
  {"SIG57", "Real-time event 57"},
  {"SIG58", "Real-time event 58"},
  {"SIG59", "Real-time event 59"},
  {"SIG60", "Real-time event 60"},
  {"SIG61", "Real-time event 61"},
  {"SIG62", "Real-time event 62"},
  {"SIG63", "Real-time event 63"},

#if defined(MACH) || defined(__MACH__)
  /* Mach exceptions */
  {"EXC_BAD_ACCESS", "Could not access memory"},
  {"EXC_BAD_INSTRUCTION", "Illegal instruction/operand"},
  {"EXC_ARITHMETIC", "Arithmetic exception"},
  {"EXC_EMULATION", "Emulation instruction"},
  {"EXC_SOFTWARE", "Software generated exception"},
  {"EXC_BREAKPOINT", "Breakpoint"},
#endif
  {NULL, "Unknown signal"},
  {NULL, "Internal error: printing TARGET_SIGNAL_DEFAULT"},

  /* Last entry, used to check whether the table is the right size.  */
  {NULL, "TARGET_SIGNAL_MAGIC"}
};

/* Return the string for a signal.  */
char *
target_signal_to_string (sig)
     enum target_signal sig;
{
  return signals[sig].string;
}

/* Return the name for a signal.  */
char *
target_signal_to_name (sig)
     enum target_signal sig;
{
  if (sig == TARGET_SIGNAL_UNKNOWN)
    /* I think the code which prints this will always print it along with
       the string, so no need to be verbose.  */
    return "?";
  return signals[sig].name;
}

/* Given a name, return its signal.  */
enum target_signal
target_signal_from_name (name)
     char *name;
{
  enum target_signal sig;

  /* It's possible we also should allow "SIGCLD" as well as "SIGCHLD"
     for TARGET_SIGNAL_SIGCHLD.  SIGIOT, on the other hand, is more
     questionable; seems like by now people should call it SIGABRT
     instead.  */

  /* This ugly cast brought to you by the native VAX compiler.  */
  for (sig = TARGET_SIGNAL_HUP;
       signals[sig].name != NULL;
       sig = (enum target_signal)((int)sig + 1))
    if (STREQ (name, signals[sig].name))
      return sig;
  return TARGET_SIGNAL_UNKNOWN;
}

/* The following functions are to help certain targets deal
   with the signal/waitstatus stuff.  They could just as well be in
   a file called native-utils.c or unixwaitstatus-utils.c or whatever.  */

/* Convert host signal to our signals.  */
enum target_signal
target_signal_from_host (hostsig)
     int hostsig;
{
  /* A switch statement would make sense but would require special kludges
     to deal with the cases where more than one signal has the same number.  */

  if (hostsig == 0) return TARGET_SIGNAL_0;

#if defined (SIGHUP)
  if (hostsig == SIGHUP) return TARGET_SIGNAL_HUP;
#endif
#if defined (SIGINT)
  if (hostsig == SIGINT) return TARGET_SIGNAL_INT;
#endif
#if defined (SIGQUIT)
  if (hostsig == SIGQUIT) return TARGET_SIGNAL_QUIT;
#endif
#if defined (SIGILL)
  if (hostsig == SIGILL) return TARGET_SIGNAL_ILL;
#endif
#if defined (SIGTRAP)
  if (hostsig == SIGTRAP) return TARGET_SIGNAL_TRAP;
#endif
#if defined (SIGABRT)
  if (hostsig == SIGABRT) return TARGET_SIGNAL_ABRT;
#endif
#if defined (SIGEMT)
  if (hostsig == SIGEMT) return TARGET_SIGNAL_EMT;
#endif
#if defined (SIGFPE)
  if (hostsig == SIGFPE) return TARGET_SIGNAL_FPE;
#endif
#if defined (SIGKILL)
  if (hostsig == SIGKILL) return TARGET_SIGNAL_KILL;
#endif
#if defined (SIGBUS)
  if (hostsig == SIGBUS) return TARGET_SIGNAL_BUS;
#endif
#if defined (SIGSEGV)
  if (hostsig == SIGSEGV) return TARGET_SIGNAL_SEGV;
#endif
#if defined (SIGSYS)
  if (hostsig == SIGSYS) return TARGET_SIGNAL_SYS;
#endif
#if defined (SIGPIPE)
  if (hostsig == SIGPIPE) return TARGET_SIGNAL_PIPE;
#endif
#if defined (SIGALRM)
  if (hostsig == SIGALRM) return TARGET_SIGNAL_ALRM;
#endif
#if defined (SIGTERM)
  if (hostsig == SIGTERM) return TARGET_SIGNAL_TERM;
#endif
#if defined (SIGUSR1)
  if (hostsig == SIGUSR1) return TARGET_SIGNAL_USR1;
#endif
#if defined (SIGUSR2)
  if (hostsig == SIGUSR2) return TARGET_SIGNAL_USR2;
#endif
#if defined (SIGCLD)
  if (hostsig == SIGCLD) return TARGET_SIGNAL_CHLD;
#endif
#if defined (SIGCHLD)
  if (hostsig == SIGCHLD) return TARGET_SIGNAL_CHLD;
#endif
#if defined (SIGPWR)
  if (hostsig == SIGPWR) return TARGET_SIGNAL_PWR;
#endif
#if defined (SIGWINCH)
  if (hostsig == SIGWINCH) return TARGET_SIGNAL_WINCH;
#endif
#if defined (SIGURG)
  if (hostsig == SIGURG) return TARGET_SIGNAL_URG;
#endif
#if defined (SIGIO)
  if (hostsig == SIGIO) return TARGET_SIGNAL_IO;
#endif
#if defined (SIGPOLL)
  if (hostsig == SIGPOLL) return TARGET_SIGNAL_POLL;
#endif
#if defined (SIGSTOP)
  if (hostsig == SIGSTOP) return TARGET_SIGNAL_STOP;
#endif
#if defined (SIGTSTP)
  if (hostsig == SIGTSTP) return TARGET_SIGNAL_TSTP;
#endif
#if defined (SIGCONT)
  if (hostsig == SIGCONT) return TARGET_SIGNAL_CONT;
#endif
#if defined (SIGTTIN)
  if (hostsig == SIGTTIN) return TARGET_SIGNAL_TTIN;
#endif
#if defined (SIGTTOU)
  if (hostsig == SIGTTOU) return TARGET_SIGNAL_TTOU;
#endif
#if defined (SIGVTALRM)
  if (hostsig == SIGVTALRM) return TARGET_SIGNAL_VTALRM;
#endif
#if defined (SIGPROF)
  if (hostsig == SIGPROF) return TARGET_SIGNAL_PROF;
#endif
#if defined (SIGXCPU)
  if (hostsig == SIGXCPU) return TARGET_SIGNAL_XCPU;
#endif
#if defined (SIGXFSZ)
  if (hostsig == SIGXFSZ) return TARGET_SIGNAL_XFSZ;
#endif
#if defined (SIGWIND)
  if (hostsig == SIGWIND) return TARGET_SIGNAL_WIND;
#endif
#if defined (SIGPHONE)
  if (hostsig == SIGPHONE) return TARGET_SIGNAL_PHONE;
#endif
#if defined (SIGLOST)
  if (hostsig == SIGLOST) return TARGET_SIGNAL_LOST;
#endif
#if defined (SIGWAITING)
  if (hostsig == SIGWAITING) return TARGET_SIGNAL_WAITING;
#endif
#if defined (SIGLWP)
  if (hostsig == SIGLWP) return TARGET_SIGNAL_LWP;
#endif
#if defined (SIGDANGER)
  if (hostsig == SIGDANGER) return TARGET_SIGNAL_DANGER;
#endif
#if defined (SIGGRANT)
  if (hostsig == SIGGRANT) return TARGET_SIGNAL_GRANT;
#endif
#if defined (SIGRETRACT)
  if (hostsig == SIGRETRACT) return TARGET_SIGNAL_RETRACT;
#endif
#if defined (SIGMSG)
  if (hostsig == SIGMSG) return TARGET_SIGNAL_MSG;
#endif
#if defined (SIGSOUND)
  if (hostsig == SIGSOUND) return TARGET_SIGNAL_SOUND;
#endif
#if defined (SIGSAK)
  if (hostsig == SIGSAK) return TARGET_SIGNAL_SAK;
#endif
#if defined (SIGPRIO)
  if (hostsig == SIGPRIO) return TARGET_SIGNAL_PRIO;
#endif

  /* Mach exceptions.  Assumes that the values for EXC_ are positive! */
#if defined (EXC_BAD_ACCESS) && defined (_NSIG)
  if (hostsig == _NSIG + EXC_BAD_ACCESS) return TARGET_EXC_BAD_ACCESS;
#endif
#if defined (EXC_BAD_INSTRUCTION) && defined (_NSIG)
  if (hostsig == _NSIG + EXC_BAD_INSTRUCTION) return TARGET_EXC_BAD_INSTRUCTION;
#endif
#if defined (EXC_ARITHMETIC) && defined (_NSIG)
  if (hostsig == _NSIG + EXC_ARITHMETIC) return TARGET_EXC_ARITHMETIC;
#endif
#if defined (EXC_EMULATION) && defined (_NSIG)
  if (hostsig == _NSIG + EXC_EMULATION) return TARGET_EXC_EMULATION;
#endif
#if defined (EXC_SOFTWARE) && defined (_NSIG)
  if (hostsig == _NSIG + EXC_SOFTWARE) return TARGET_EXC_SOFTWARE;
#endif
#if defined (EXC_BREAKPOINT) && defined (_NSIG)
  if (hostsig == _NSIG + EXC_BREAKPOINT) return TARGET_EXC_BREAKPOINT;
#endif

#if defined (REALTIME_LO)
  if (hostsig >= REALTIME_LO && hostsig < REALTIME_HI)
    return (enum target_signal)
      (hostsig - 33 + (int) TARGET_SIGNAL_REALTIME_33);
#endif
  return TARGET_SIGNAL_UNKNOWN;
}

int
target_signal_to_host (oursig)
     enum target_signal oursig;
{
  switch (oursig)
    {
    case TARGET_SIGNAL_0: return 0;

#if defined (SIGHUP)
    case TARGET_SIGNAL_HUP: return SIGHUP;
#endif
#if defined (SIGINT)
    case TARGET_SIGNAL_INT: return SIGINT;
#endif
#if defined (SIGQUIT)
    case TARGET_SIGNAL_QUIT: return SIGQUIT;
#endif
#if defined (SIGILL)
    case TARGET_SIGNAL_ILL: return SIGILL;
#endif
#if defined (SIGTRAP)
    case TARGET_SIGNAL_TRAP: return SIGTRAP;
#endif
#if defined (SIGABRT)
    case TARGET_SIGNAL_ABRT: return SIGABRT;
#endif
#if defined (SIGEMT)
    case TARGET_SIGNAL_EMT: return SIGEMT;
#endif
#if defined (SIGFPE)
    case TARGET_SIGNAL_FPE: return SIGFPE;
#endif
#if defined (SIGKILL)
    case TARGET_SIGNAL_KILL: return SIGKILL;
#endif
#if defined (SIGBUS)
    case TARGET_SIGNAL_BUS: return SIGBUS;
#endif
#if defined (SIGSEGV)
    case TARGET_SIGNAL_SEGV: return SIGSEGV;
#endif
#if defined (SIGSYS)
    case TARGET_SIGNAL_SYS: return SIGSYS;
#endif
#if defined (SIGPIPE)
    case TARGET_SIGNAL_PIPE: return SIGPIPE;
#endif
#if defined (SIGALRM)
    case TARGET_SIGNAL_ALRM: return SIGALRM;
#endif
#if defined (SIGTERM)
    case TARGET_SIGNAL_TERM: return SIGTERM;
#endif
#if defined (SIGUSR1)
    case TARGET_SIGNAL_USR1: return SIGUSR1;
#endif
#if defined (SIGUSR2)
    case TARGET_SIGNAL_USR2: return SIGUSR2;
#endif
#if defined (SIGCHLD) || defined (SIGCLD)
    case TARGET_SIGNAL_CHLD: 
#if defined (SIGCHLD)
      return SIGCHLD;
#else
      return SIGCLD;
#endif
#endif /* SIGCLD or SIGCHLD */
#if defined (SIGPWR)
    case TARGET_SIGNAL_PWR: return SIGPWR;
#endif
#if defined (SIGWINCH)
    case TARGET_SIGNAL_WINCH: return SIGWINCH;
#endif
#if defined (SIGURG)
    case TARGET_SIGNAL_URG: return SIGURG;
#endif
#if defined (SIGIO)
    case TARGET_SIGNAL_IO: return SIGIO;
#endif
#if defined (SIGPOLL)
    case TARGET_SIGNAL_POLL: return SIGPOLL;
#endif
#if defined (SIGSTOP)
    case TARGET_SIGNAL_STOP: return SIGSTOP;
#endif
#if defined (SIGTSTP)
    case TARGET_SIGNAL_TSTP: return SIGTSTP;
#endif
#if defined (SIGCONT)
    case TARGET_SIGNAL_CONT: return SIGCONT;
#endif
#if defined (SIGTTIN)
    case TARGET_SIGNAL_TTIN: return SIGTTIN;
#endif
#if defined (SIGTTOU)
    case TARGET_SIGNAL_TTOU: return SIGTTOU;
#endif
#if defined (SIGVTALRM)
    case TARGET_SIGNAL_VTALRM: return SIGVTALRM;
#endif
#if defined (SIGPROF)
    case TARGET_SIGNAL_PROF: return SIGPROF;
#endif
#if defined (SIGXCPU)
    case TARGET_SIGNAL_XCPU: return SIGXCPU;
#endif
#if defined (SIGXFSZ)
    case TARGET_SIGNAL_XFSZ: return SIGXFSZ;
#endif
#if defined (SIGWIND)
    case TARGET_SIGNAL_WIND: return SIGWIND;
#endif
#if defined (SIGPHONE)
    case TARGET_SIGNAL_PHONE: return SIGPHONE;
#endif
#if defined (SIGLOST)
    case TARGET_SIGNAL_LOST: return SIGLOST;
#endif
#if defined (SIGWAITING)
    case TARGET_SIGNAL_WAITING: return SIGWAITING;
#endif
#if defined (SIGLWP)
    case TARGET_SIGNAL_LWP: return SIGLWP;
#endif
#if defined (SIGDANGER)
    case TARGET_SIGNAL_DANGER: return SIGDANGER;
#endif
#if defined (SIGGRANT)
    case TARGET_SIGNAL_GRANT: return SIGGRANT;
#endif
#if defined (SIGRETRACT)
    case TARGET_SIGNAL_RETRACT: return SIGRETRACT;
#endif
#if defined (SIGMSG)
    case TARGET_SIGNAL_MSG: return SIGMSG;
#endif
#if defined (SIGSOUND)
    case TARGET_SIGNAL_SOUND: return SIGSOUND;
#endif
#if defined (SIGSAK)
    case TARGET_SIGNAL_SAK: return SIGSAK;
#endif
#if defined (SIGPRIO)
    case TARGET_SIGNAL_PRIO: return SIGPRIO;
#endif

      /* Mach exceptions.  Assumes that the values for EXC_ are positive! */
#if defined (EXC_BAD_ACCESS) && defined (_NSIG)
    case TARGET_EXC_BAD_ACCESS: return _NSIG + EXC_BAD_ACCESS;
#endif
#if defined (EXC_BAD_INSTRUCTION) && defined (_NSIG)
    case TARGET_EXC_BAD_INSTRUCTION: return _NSIG + EXC_BAD_INSTRUCTION;
#endif
#if defined (EXC_ARITHMETIC) && defined (_NSIG)
    case TARGET_EXC_ARITHMETIC: return _NSIG + EXC_ARITHMETIC;
#endif
#if defined (EXC_EMULATION) && defined (_NSIG)
    case TARGET_EXC_EMULATION: return _NSIG + EXC_EMULATION;
#endif
#if defined (EXC_SOFTWARE) && defined (_NSIG)
    case TARGET_EXC_SOFTWARE: return _NSIG + EXC_SOFTWARE;
#endif
#if defined (EXC_BREAKPOINT) && defined (_NSIG)
    case TARGET_EXC_BREAKPOINT: return _NSIG + EXC_BREAKPOINT;
#endif

    default:
#if defined (REALTIME_LO)
      if (oursig >= TARGET_SIGNAL_REALTIME_33
	  && oursig <= TARGET_SIGNAL_REALTIME_63)
	{
	  int retsig =
	    (int)oursig - (int)TARGET_SIGNAL_REALTIME_33 + REALTIME_LO;
	  if (retsig < REALTIME_HI)
	    return retsig;
	}
#endif
      /* The user might be trying to do "signal SIGSAK" where this system
	 doesn't have SIGSAK.  */
      warning ("Signal %s does not exist on this system.\n",
	       target_signal_to_name (oursig));
      return 0;
    }
}

/* Helper function for child_wait and the Lynx derivatives of child_wait.
   HOSTSTATUS is the waitstatus from wait() or the equivalent; store our
   translation of that in OURSTATUS.  */
void
store_waitstatus (ourstatus, hoststatus)
     struct target_waitstatus *ourstatus;
     int hoststatus;
{
#ifdef CHILD_SPECIAL_WAITSTATUS
  /* CHILD_SPECIAL_WAITSTATUS should return nonzero and set *OURSTATUS
     if it wants to deal with hoststatus.  */
  if (CHILD_SPECIAL_WAITSTATUS (ourstatus, hoststatus))
    return;
#endif

  if (WIFEXITED (hoststatus))
    {
      ourstatus->kind = TARGET_WAITKIND_EXITED;
      ourstatus->value.integer = WEXITSTATUS (hoststatus);
    }
  else if (!WIFSTOPPED (hoststatus))
    {
      ourstatus->kind = TARGET_WAITKIND_SIGNALLED;
      ourstatus->value.sig = target_signal_from_host (WTERMSIG (hoststatus));
    }
  else
    {
      ourstatus->kind = TARGET_WAITKIND_STOPPED;
      ourstatus->value.sig = target_signal_from_host (WSTOPSIG (hoststatus));
    }
}

/* In some circumstances we allow a command to specify a numeric
   signal.  The idea is to keep these circumstances limited so that
   users (and scripts) develop portable habits.  For comparison,
   POSIX.2 `kill' requires that 1,2,3,6,9,14, and 15 work (and using a
   numeric signal at all is obscelescent.  We are slightly more
   lenient and allow 1-15 which should match host signal numbers on
   most systems.  Use of symbolic signal names is strongly encouraged.  */

enum target_signal
target_signal_from_command (num)
     int num;
{
  if (num >= 1 && num <= 15)
    return (enum target_signal)num;
  error ("Only signals 1-15 are valid as numeric signals.\n\
Use \"info signals\" for a list of symbolic signals.");
}

/* Returns zero to leave the inferior alone, one to interrupt it.  */
int (*target_activity_function) PARAMS ((void));
int target_activity_fd;

/* Convert a normal process ID to a string.  Returns the string in a static
   buffer.  */

char *
normal_pid_to_str (pid)
     int pid;
{
  static char buf[30];

  if (STREQ (current_target.to_shortname, "remote"))
    sprintf (buf, "thread %d\0", pid);
  else
    sprintf (buf, "process %d\0", pid);

  return buf;
}

/* Some targets (such as ttrace-based HPUX) don't allow us to request
   notification of inferior events such as fork and vork immediately
   after the inferior is created.  (This because of how gdb gets an
   inferior created via invoking a shell to do it.  In such a scenario,
   if the shell init file has commands in it, the shell will fork and
   exec for each of those commands, and we will see each such fork
   event.  Very bad.)
   
   This function is used by all targets that allow us to request
   notification of forks, etc at inferior creation time; e.g., in
   target_acknowledge_forked_child.
   */
void
normal_target_post_startup_inferior (pid)
  int  pid;
{
  /* This space intentionally left blank. */
}

/* Set up the handful of non-empty slots needed by the dummy target
   vector.  */

static void
init_dummy_target ()
{
  dummy_target.to_shortname = "None";
  dummy_target.to_longname = "None";
  dummy_target.to_doc = "";
  dummy_target.to_attach = find_default_attach;
  dummy_target.to_require_attach = find_default_require_attach;
  dummy_target.to_require_detach = find_default_require_detach;
  dummy_target.to_create_inferior = find_default_create_inferior;
  dummy_target.to_clone_and_follow_inferior = find_default_clone_and_follow_inferior;
  dummy_target.to_stratum = dummy_stratum;
  dummy_target.to_magic = OPS_MAGIC;
}


#ifdef MAINTENANCE_CMDS
static struct target_ops debug_target;

static void
debug_to_open (args, from_tty)
     char *args;
     int from_tty;
{
  debug_target.to_open (args, from_tty);

  fprintf_unfiltered (gdb_stderr, "target_open (%s, %d)\n", args, from_tty);
}

static void
debug_to_close (quitting)
     int quitting;
{
  debug_target.to_close (quitting);

  fprintf_unfiltered (gdb_stderr, "target_close (%d)\n", quitting);
}

static void
debug_to_attach (args, from_tty)
     char *args;
     int from_tty;
{
  debug_target.to_attach (args, from_tty);

  fprintf_unfiltered (gdb_stderr, "target_attach (%s, %d)\n", args, from_tty);
}


static void
debug_to_post_attach (pid)
  int  pid;
{
  debug_target.to_post_attach (pid);

  fprintf_unfiltered (gdb_stderr, "target_post_attach (%d)\n", pid);
}

static void
debug_to_require_attach (args, from_tty)
     char *args;
     int from_tty;
{
  debug_target.to_require_attach (args, from_tty);

  fprintf_unfiltered (gdb_stderr,
		      "target_require_attach (%s, %d)\n", args, from_tty);
}

static void
debug_to_detach (args, from_tty)
     char *args;
     int from_tty;
{
  debug_target.to_detach (args, from_tty);

  fprintf_unfiltered (gdb_stderr, "target_detach (%s, %d)\n", args, from_tty);
}

static void
debug_to_require_detach (pid, args, from_tty)
  int  pid;
  char *  args;
  int  from_tty;
{
  debug_target.to_require_detach (pid, args, from_tty);

  fprintf_unfiltered (gdb_stderr,
		      "target_require_detach (%d, %s, %d)\n", pid, args, from_tty);
}

static void
debug_to_resume (pid, step, siggnal)
     int pid;
     int step;
     enum target_signal siggnal;
{
  debug_target.to_resume (pid, step, siggnal);

  fprintf_unfiltered (gdb_stderr, "target_resume (%d, %s, %s)\n", pid,
		      step ? "step" : "continue",
		      target_signal_to_name (siggnal));
}

static int
debug_to_wait (pid, status)
     int pid;
     struct target_waitstatus *status;
{
  int retval;

  retval = debug_target.to_wait (pid, status);

  fprintf_unfiltered (gdb_stderr,
		      "target_wait (%d, status) = %d,   ", pid, retval);
  fprintf_unfiltered (gdb_stderr, "status->kind = ");
  switch (status->kind)
    {
    case TARGET_WAITKIND_EXITED:
      fprintf_unfiltered (gdb_stderr, "exited, status = %d\n",
			  status->value.integer);
      break;
    case TARGET_WAITKIND_STOPPED:
      fprintf_unfiltered (gdb_stderr, "stopped, signal = %s\n",
			  target_signal_to_name (status->value.sig));
      break;
    case TARGET_WAITKIND_SIGNALLED:
      fprintf_unfiltered (gdb_stderr, "signalled, signal = %s\n",
			  target_signal_to_name (status->value.sig));
      break;
    case TARGET_WAITKIND_LOADED:
      fprintf_unfiltered (gdb_stderr, "loaded\n");
      break;
    case TARGET_WAITKIND_FORKED:
      fprintf_unfiltered (gdb_stderr, "forked\n");
      break;
    case TARGET_WAITKIND_VFORKED:
      fprintf_unfiltered (gdb_stderr, "vforked\n");
      break;
    case TARGET_WAITKIND_EXECD:
      fprintf_unfiltered (gdb_stderr, "execd\n");
      break;
    case TARGET_WAITKIND_SPURIOUS:
      fprintf_unfiltered (gdb_stderr, "spurious\n");
      break;
    default:
      fprintf_unfiltered (gdb_stderr, "unknown???\n");
      break;
    }

  return retval;
}

static void
debug_to_post_wait (pid, status)
  int  pid;
  int  status;
{
  debug_target.to_post_wait (pid, status);

  fprintf_unfiltered (gdb_stderr, "target_post_wait (%d, %d)\n",
		      pid, status);
}

static void
debug_to_fetch_registers (regno)
     int regno;
{
  debug_target.to_fetch_registers (regno);

  fprintf_unfiltered (gdb_stderr, "target_fetch_registers (%s)",
		      regno != -1 ? REGISTER_NAME (regno) : "-1");
  if (regno != -1)
    fprintf_unfiltered (gdb_stderr, " = 0x%x %d",
			(unsigned long) read_register (regno),
			read_register (regno));
  fprintf_unfiltered (gdb_stderr, "\n");
}

static void
debug_to_store_registers (regno)
     int regno;
{
  debug_target.to_store_registers (regno);

  if (regno >= 0 && regno < NUM_REGS)
    fprintf_unfiltered (gdb_stderr, "target_store_registers (%s) = 0x%x %d\n",
			REGISTER_NAME (regno),
			(unsigned long) read_register (regno),
			(unsigned long) read_register (regno));
  else
    fprintf_unfiltered (gdb_stderr, "target_store_registers (%d)\n", regno);
}

static void
debug_to_prepare_to_store ()
{
  debug_target.to_prepare_to_store ();

  fprintf_unfiltered (gdb_stderr, "target_prepare_to_store ()\n");
}

static int
debug_to_xfer_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;
{
  int retval;

  retval = debug_target.to_xfer_memory (memaddr, myaddr, len, write, target);

  fprintf_unfiltered (gdb_stderr,
		      "target_xfer_memory (0x%x, xxx, %d, %s, xxx) = %d",
		      (unsigned int) memaddr, /* possable truncate long long */
		      len, write ? "write" : "read", retval);

  

  if (retval > 0)
    {
      int i;

      fputs_unfiltered (", bytes =", gdb_stderr);
      for (i = 0; i < retval; i++)
	{
	  if ((((long) &(myaddr[i])) & 0xf) == 0)
	    fprintf_unfiltered (gdb_stderr, "\n");
	  fprintf_unfiltered (gdb_stderr, " %02x", myaddr[i] & 0xff);
	}
    }

  fputc_unfiltered ('\n', gdb_stderr);

  return retval;
}

static void
debug_to_files_info (target)
     struct target_ops *target;
{
  debug_target.to_files_info (target);

  fprintf_unfiltered (gdb_stderr, "target_files_info (xxx)\n");
}

static int
debug_to_insert_breakpoint (addr, save)
     CORE_ADDR addr;
     char *save;
{
  int retval;

  retval = debug_target.to_insert_breakpoint (addr, save);

  fprintf_unfiltered (gdb_stderr,
		      "target_insert_breakpoint (0x%x, xxx) = %d\n",
		      (unsigned long) addr, retval);
  return retval;
}

static int
debug_to_remove_breakpoint (addr, save)
     CORE_ADDR addr;
     char *save;
{
  int retval;

  retval = debug_target.to_remove_breakpoint (addr, save);

  fprintf_unfiltered (gdb_stderr,
		      "target_remove_breakpoint (0x%x, xxx) = %d\n",
		      (unsigned long)addr, retval);
  return retval;
}

static void
debug_to_terminal_init ()
{
  debug_target.to_terminal_init ();

  fprintf_unfiltered (gdb_stderr, "target_terminal_init ()\n");
}

static void
debug_to_terminal_inferior ()
{
  debug_target.to_terminal_inferior ();

  fprintf_unfiltered (gdb_stderr, "target_terminal_inferior ()\n");
}

static void
debug_to_terminal_ours_for_output ()
{
  debug_target.to_terminal_ours_for_output ();

  fprintf_unfiltered (gdb_stderr, "target_terminal_ours_for_output ()\n");
}

static void
debug_to_terminal_ours ()
{
  debug_target.to_terminal_ours ();

  fprintf_unfiltered (gdb_stderr, "target_terminal_ours ()\n");
}

static void
debug_to_terminal_info (arg, from_tty)
     char *arg;
     int from_tty;
{
  debug_target.to_terminal_info (arg, from_tty);

  fprintf_unfiltered (gdb_stderr, "target_terminal_info (%s, %d)\n", arg,
		      from_tty);
}

static void
debug_to_kill ()
{
  debug_target.to_kill ();

  fprintf_unfiltered (gdb_stderr, "target_kill ()\n");
}

static void
debug_to_load (args, from_tty)
     char *args;
     int from_tty;
{
  debug_target.to_load (args, from_tty);

  fprintf_unfiltered (gdb_stderr, "target_load (%s, %d)\n", args, from_tty);
}

static int
debug_to_lookup_symbol (name, addrp)
     char *name;
     CORE_ADDR *addrp;
{
  int retval;

  retval = debug_target.to_lookup_symbol (name, addrp);

  fprintf_unfiltered (gdb_stderr, "target_lookup_symbol (%s, xxx)\n", name);

  return retval;
}

static void
debug_to_create_inferior (exec_file, args, env)
     char *exec_file;
     char *args;
     char **env;
{
  debug_target.to_create_inferior (exec_file, args, env);

  fprintf_unfiltered (gdb_stderr, "target_create_inferior (%s, %s, xxx)\n",
		      exec_file, args);
}

static void
debug_to_post_startup_inferior (pid)
  int  pid;
{
  debug_target.to_post_startup_inferior (pid);

  fprintf_unfiltered (gdb_stderr, "target_post_startup_inferior (%d)\n",
		      pid);
}

static void
debug_to_acknowledge_created_inferior (pid)
  int  pid;
{
  debug_target.to_acknowledge_created_inferior (pid);

  fprintf_unfiltered (gdb_stderr, "target_acknowledge_created_inferior (%d)\n",
		      pid);
}

static void
debug_to_clone_and_follow_inferior (child_pid, followed_child)
  int  child_pid;
  int  *followed_child;
{
  debug_target.to_clone_and_follow_inferior (child_pid, followed_child);

  fprintf_unfiltered (gdb_stderr,
		      "target_clone_and_follow_inferior (%d, %d)\n",
		      child_pid, *followed_child);
}

static void
debug_to_post_follow_inferior_by_clone ()
{
  debug_target.to_post_follow_inferior_by_clone ();

  fprintf_unfiltered (gdb_stderr, "target_post_follow_inferior_by_clone ()\n");
}

static int
debug_to_insert_fork_catchpoint (pid)
  int  pid;
{
  int  retval;

  retval = debug_target.to_insert_fork_catchpoint (pid);

  fprintf_unfiltered (gdb_stderr, "target_insert_fork_catchpoint (%d) = %d\n",
                      pid, retval);

  return retval;
}

static int
debug_to_remove_fork_catchpoint (pid)
  int  pid;
{
  int  retval;

  retval = debug_target.to_remove_fork_catchpoint (pid);

  fprintf_unfiltered (gdb_stderr, "target_remove_fork_catchpoint (%d) = %d\n",
                      pid, retval);

  return retval;
}

static int
debug_to_insert_vfork_catchpoint (pid)
  int  pid;
{
  int  retval;

  retval = debug_target.to_insert_vfork_catchpoint (pid);

  fprintf_unfiltered (gdb_stderr, "target_insert_vfork_catchpoint (%d)= %d\n",
                      pid, retval);

  return retval;
}

static int
debug_to_remove_vfork_catchpoint (pid)
  int  pid;
{
  int  retval;

  retval = debug_target.to_remove_vfork_catchpoint (pid);

  fprintf_unfiltered (gdb_stderr, "target_remove_vfork_catchpoint (%d) = %d\n",
                      pid, retval);

  return retval;
}

static int
debug_to_has_forked (pid, child_pid)
  int  pid;
  int *  child_pid;
{
  int  has_forked;

  has_forked = debug_target.to_has_forked (pid, child_pid);

  fprintf_unfiltered (gdb_stderr, "target_has_forked (%d, %d) = %d\n",
                      pid, *child_pid, has_forked);

  return has_forked;
}

static int
debug_to_has_vforked (pid, child_pid)
  int  pid;
  int *  child_pid;
{
  int  has_vforked;

  has_vforked = debug_target.to_has_vforked (pid, child_pid);

  fprintf_unfiltered (gdb_stderr, "target_has_vforked (%d, %d) = %d\n",
                      pid, *child_pid, has_vforked);

  return has_vforked;
}

static int
debug_to_can_follow_vfork_prior_to_exec ()
{
  int  can_immediately_follow_vfork;

  can_immediately_follow_vfork = debug_target.to_can_follow_vfork_prior_to_exec ();

  fprintf_unfiltered (gdb_stderr, "target_can_follow_vfork_prior_to_exec () = %d\n",
                      can_immediately_follow_vfork);

  return can_immediately_follow_vfork;
}

static void
debug_to_post_follow_vfork (parent_pid, followed_parent, child_pid, followed_child)
  int  parent_pid;
  int  followed_parent;
  int  child_pid;
  int  followed_child;
{
  debug_target.to_post_follow_vfork (parent_pid, followed_parent, child_pid, followed_child);

  fprintf_unfiltered (gdb_stderr,
		      "target_post_follow_vfork (%d, %d, %d, %d)\n",
                      parent_pid, followed_parent, child_pid, followed_child);
}

static int
debug_to_insert_exec_catchpoint (pid)
  int  pid;
{
  int  retval;

  retval = debug_target.to_insert_exec_catchpoint (pid);

  fprintf_unfiltered (gdb_stderr, "target_insert_exec_catchpoint (%d) = %d\n",
                      pid, retval);

  return retval;
}

static int
debug_to_remove_exec_catchpoint (pid)
  int  pid;
{
  int  retval;

  retval = debug_target.to_remove_exec_catchpoint (pid);

  fprintf_unfiltered (gdb_stderr, "target_remove_exec_catchpoint (%d) = %d\n",
                      pid, retval);

  return retval;
}

static int
debug_to_has_execd (pid, execd_pathname)
  int  pid;
  char **  execd_pathname;
{
  int  has_execd;

  has_execd = debug_target.to_has_execd (pid, execd_pathname);

  fprintf_unfiltered (gdb_stderr, "target_has_execd (%d, %s) = %d\n",
                      pid, *execd_pathname, has_execd);

  return has_execd;
}

static int
debug_to_reported_exec_events_per_exec_call ()
{
  int  reported_exec_events;

  reported_exec_events = debug_target.to_reported_exec_events_per_exec_call ();

  fprintf_unfiltered (gdb_stderr,
		      "target_reported_exec_events_per_exec_call () = %d\n",
                      reported_exec_events);

  return reported_exec_events;
}

static int
debug_to_has_syscall_event (pid, kind, syscall_id)
  int  pid;
  enum target_waitkind *  kind;
  int *  syscall_id;
{
  int  has_syscall_event;
  char *  kind_spelling = "??";

  has_syscall_event = debug_target.to_has_syscall_event (pid, kind, syscall_id);
  if (has_syscall_event)
    {
      switch (*kind)
        {
          case TARGET_WAITKIND_SYSCALL_ENTRY:
            kind_spelling = "SYSCALL_ENTRY";
            break;
          case TARGET_WAITKIND_SYSCALL_RETURN:
            kind_spelling = "SYSCALL_RETURN";
            break;
          default:
            break;
        }
    }

  fprintf_unfiltered (gdb_stderr,
		      "target_has_syscall_event (%d, %s, %d) = %d\n",
                      pid, kind_spelling, *syscall_id, has_syscall_event);

  return has_syscall_event;
}

static int
debug_to_has_exited (pid, wait_status, exit_status)
  int  pid;
  int  wait_status;
  int *  exit_status;
{
  int  has_exited;

  has_exited = debug_target.to_has_exited (pid, wait_status, exit_status);

  fprintf_unfiltered (gdb_stderr, "target_has_exited (%d, %d, %d) = %d\n",
                      pid, wait_status, *exit_status, has_exited);

  return has_exited;
}

static void
debug_to_mourn_inferior ()
{
  debug_target.to_mourn_inferior ();

  fprintf_unfiltered (gdb_stderr, "target_mourn_inferior ()\n");
}

static int
debug_to_can_run ()
{
  int retval;

  retval = debug_target.to_can_run ();

  fprintf_unfiltered (gdb_stderr, "target_can_run () = %d\n", retval);

  return retval;
}

static void
debug_to_notice_signals (pid)
     int pid;
{
  debug_target.to_notice_signals (pid);

  fprintf_unfiltered (gdb_stderr, "target_notice_signals (%d)\n", pid);
}

static int
debug_to_thread_alive (pid)
     int pid;
{
  int retval;

  retval = debug_target.to_thread_alive (pid);

  fprintf_unfiltered (gdb_stderr, "target_thread_alive (%d) = %d\n",
		      pid, retval);

  return retval;
}

static void
debug_to_stop ()
{
  debug_target.to_stop ();

  fprintf_unfiltered (gdb_stderr, "target_stop ()\n");
}

static int
debug_to_query (type, req, resp, siz)
  int type;
  char *req;
  char *resp;
  int *siz;
{
  int retval;

  retval = debug_target.to_query (type, req, resp, siz);

  fprintf_unfiltered (gdb_stderr, "target_query (%c, %s, %s,  %d) = %d\n", type, req, resp, *siz, retval);

  return retval;
}

static struct symtab_and_line *
debug_to_enable_exception_callback (kind, enable)
  enum exception_event_kind kind;
  int enable;
{
  debug_target.to_enable_exception_callback (kind, enable);

  fprintf_unfiltered (gdb_stderr,
		      "target get_exception_callback_sal (%d, %d)\n",
		      kind, enable);
}

static struct exception_event_record *
debug_to_get_current_exception_event ()
{
  debug_target.to_get_current_exception_event();

  fprintf_unfiltered (gdb_stderr, "target get_current_exception_event ()\n");
}

static char *
debug_to_pid_to_exec_file (pid)
  int  pid;
{
  char *  exec_file;

  exec_file = debug_target.to_pid_to_exec_file (pid);

  fprintf_unfiltered (gdb_stderr, "target_pid_to_exec_file (%d) = %s\n",
                      pid, exec_file);

  return exec_file;
}

static char *
debug_to_core_file_to_sym_file (core)
  char *  core;
{
  char *  sym_file;

  sym_file = debug_target.to_core_file_to_sym_file (core);

  fprintf_unfiltered (gdb_stderr, "target_core_file_to_sym_file (%s) = %s\n",
                      core, sym_file);

  return sym_file;
}

static void
setup_target_debug ()
{
  memcpy (&debug_target, &current_target, sizeof debug_target);

  current_target.to_open = debug_to_open;
  current_target.to_close = debug_to_close;
  current_target.to_attach = debug_to_attach;
  current_target.to_post_attach = debug_to_post_attach;
  current_target.to_require_attach = debug_to_require_attach;
  current_target.to_detach = debug_to_detach;
  current_target.to_require_detach = debug_to_require_detach;
  current_target.to_resume = debug_to_resume;
  current_target.to_wait = debug_to_wait;
  current_target.to_post_wait = debug_to_post_wait;
  current_target.to_fetch_registers = debug_to_fetch_registers;
  current_target.to_store_registers = debug_to_store_registers;
  current_target.to_prepare_to_store = debug_to_prepare_to_store;
  current_target.to_xfer_memory = debug_to_xfer_memory;
  current_target.to_files_info = debug_to_files_info;
  current_target.to_insert_breakpoint = debug_to_insert_breakpoint;
  current_target.to_remove_breakpoint = debug_to_remove_breakpoint;
  current_target.to_terminal_init = debug_to_terminal_init;
  current_target.to_terminal_inferior = debug_to_terminal_inferior;
  current_target.to_terminal_ours_for_output = debug_to_terminal_ours_for_output;
  current_target.to_terminal_ours = debug_to_terminal_ours;
  current_target.to_terminal_info = debug_to_terminal_info;
  current_target.to_kill = debug_to_kill;
  current_target.to_load = debug_to_load;
  current_target.to_lookup_symbol = debug_to_lookup_symbol;
  current_target.to_create_inferior = debug_to_create_inferior;
  current_target.to_post_startup_inferior = debug_to_post_startup_inferior;
  current_target.to_acknowledge_created_inferior = debug_to_acknowledge_created_inferior;
  current_target.to_clone_and_follow_inferior = debug_to_clone_and_follow_inferior;
  current_target.to_post_follow_inferior_by_clone = debug_to_post_follow_inferior_by_clone;
  current_target.to_insert_fork_catchpoint = debug_to_insert_fork_catchpoint;
  current_target.to_remove_fork_catchpoint = debug_to_remove_fork_catchpoint;
  current_target.to_insert_vfork_catchpoint = debug_to_insert_vfork_catchpoint;
  current_target.to_remove_vfork_catchpoint = debug_to_remove_vfork_catchpoint;
  current_target.to_has_forked = debug_to_has_forked;
  current_target.to_has_vforked = debug_to_has_vforked;
  current_target.to_can_follow_vfork_prior_to_exec = debug_to_can_follow_vfork_prior_to_exec;
  current_target.to_post_follow_vfork = debug_to_post_follow_vfork;
  current_target.to_insert_exec_catchpoint = debug_to_insert_exec_catchpoint;
  current_target.to_remove_exec_catchpoint = debug_to_remove_exec_catchpoint;
  current_target.to_has_execd = debug_to_has_execd;
  current_target.to_reported_exec_events_per_exec_call = debug_to_reported_exec_events_per_exec_call;
  current_target.to_has_syscall_event = debug_to_has_syscall_event;
  current_target.to_has_exited = debug_to_has_exited;
  current_target.to_mourn_inferior = debug_to_mourn_inferior;
  current_target.to_can_run = debug_to_can_run;
  current_target.to_notice_signals = debug_to_notice_signals;
  current_target.to_thread_alive = debug_to_thread_alive;
  current_target.to_stop = debug_to_stop;
  current_target.to_query = debug_to_query;
  current_target.to_enable_exception_callback = debug_to_enable_exception_callback;
  current_target.to_get_current_exception_event = debug_to_get_current_exception_event;
  current_target.to_pid_to_exec_file = debug_to_pid_to_exec_file;
  current_target.to_core_file_to_sym_file = debug_to_core_file_to_sym_file;

}
#endif /* MAINTENANCE_CMDS */

static char targ_desc[] = 
    "Names of targets and files being debugged.\n\
Shows the entire stack of targets currently in use (including the exec-file,\n\
core-file, and process, if any), as well as the symbol file name.";

void
initialize_targets ()
{
  init_dummy_target ();
  push_target (&dummy_target);

  add_info ("target", target_info, targ_desc);
  add_info ("files", target_info, targ_desc);

#ifdef MAINTENANCE_CMDS
  add_show_from_set (
     add_set_cmd ("targetdebug", class_maintenance, var_zinteger,
		  (char *)&targetdebug,
		 "Set target debugging.\n\
When non-zero, target debugging is enabled.", &setlist),
		     &showlist);
#endif

  if (!STREQ (signals[TARGET_SIGNAL_LAST].string, "TARGET_SIGNAL_MAGIC"))
    abort ();
}
