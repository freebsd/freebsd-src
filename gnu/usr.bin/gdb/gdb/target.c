/* Select target systems and architectures at runtime for GDB.
   Copyright 1990, 1992, 1993 Free Software Foundation, Inc.
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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "defs.h"
#include <errno.h>
#include <ctype.h>
#include "target.h"
#include "gdbcmd.h"
#include "symtab.h"
#include "inferior.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"

extern int errno;

static void
target_info PARAMS ((char *, int));

static void
cleanup_target PARAMS ((struct target_ops *));

static void
maybe_kill_then_create_inferior PARAMS ((char *, char *, char **));

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
nomemory PARAMS ((CORE_ADDR, char *, int, int));

static int
return_zero PARAMS ((void));

static void
ignore PARAMS ((void));

static void
target_command PARAMS ((char *, int));

static struct target_ops *
find_default_run_target PARAMS ((char *));

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

struct target_ops dummy_target = {"None", "None", "",
    0, 0, 		/* open, close */
    find_default_attach, 0,  /* attach, detach */
    0, 0,		/* resume, wait */
    0, 0, 0,		/* registers */
    0, 0, 		/* memory */
    0, 0, 		/* bkpts */
    0, 0, 0, 0, 0, 	/* terminal */
    0, 0, 		/* kill, load */
    0, 			/* lookup_symbol */
    find_default_create_inferior, /* create_inferior */
    0,			/* mourn_inferior */
    0,			/* can_run */
    0,			/* notice_signals */
    dummy_stratum, 0,	/* stratum, next */
    0, 0, 0, 0, 0,	/* all mem, mem, stack, regs, exec */
    0, 0,		/* section pointers */
    OPS_MAGIC,
};

/* The target structure we are currently using to talk to a process
   or file or whatever "inferior" we have.  */

struct target_ops *current_target;

/* The stack of target structures that have been pushed.  */

struct target_ops **current_target_stack;

/* Command list for target.  */

static struct cmd_list_element *targetlist = NULL;

/* The user just typed 'target' without the name of a target.  */

/* ARGSUSED */
static void
target_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  fputs_filtered ("Argument required (target name).  Try `help target'\n",
		  stdout);
}

/* Add a possible target architecture to the list.  */

void
add_target (t)
     struct target_ops *t;
{
  if (t->to_magic != OPS_MAGIC)
    {
      fprintf(stderr, "Magic number of %s target struct wrong\n", 
	t->to_shortname);
      abort();
    }

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
  cleanup_target (t);

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

static void
ignore ()
{
}

/* ARGSUSED */
static int
nomemory (memaddr, myaddr, len, write)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
{
  errno = EIO;		/* Can't read/write this location */
  return 0;		/* No bytes handled */
}

static void
tcomplain ()
{
  error ("You can't do that when your target is `%s'",
	 current_target->to_shortname);
}

void
noprocess ()
{
  error ("You can't do that without a process to debug");
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
static void
default_terminal_info (args, from_tty)
     char *args;
     int from_tty;
{
  printf("No saved terminal information.\n");
}

#if 0
/* With strata, this function is no longer needed.  FIXME.  */
/* This is the default target_create_inferior function.  It looks up
   the stack for some target that cares to create inferiors, then
   calls it -- or complains if not found.  */

static void
upstack_create_inferior (exec, args, env)
     char *exec;
     char *args;
     char **env;
{
  struct target_ops *t;

  for (t = current_target;
       t;
       t = t->to_next)
    {
      if (t->to_create_inferior != upstack_create_inferior)
	{
          t->to_create_inferior (exec, args, env);
	  return;
	}

    }
  tcomplain();
}
#endif

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
      printf ("You are already running a program:\n");
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

/* Clean up a target struct so it no longer has any zero pointers in it.
   We default entries, at least to stubs that print error messages.  */

static void
cleanup_target (t)
     struct target_ops *t;
{

  /* Check magic number.  If wrong, it probably means someone changed
     the struct definition, but not all the places that initialize one.  */
  if (t->to_magic != OPS_MAGIC)
    {
      fprintf(stderr, "Magic number of %s target struct wrong\n", 
	t->to_shortname);
      abort();
    }

#define de_fault(field, value) \
  if (!t->field)	t->field = value

  /*        FIELD			DEFAULT VALUE        */

  de_fault (to_open, 			(void (*)())tcomplain);
  de_fault (to_close, 			(void (*)())ignore);
  de_fault (to_attach, 			maybe_kill_then_attach);
  de_fault (to_detach, 			(void (*)())ignore);
  de_fault (to_resume, 			(void (*)())noprocess);
  de_fault (to_wait, 			(int (*)())noprocess);
  de_fault (to_fetch_registers, 	(void (*)())ignore);
  de_fault (to_store_registers,		(void (*)())noprocess);
  de_fault (to_prepare_to_store,	(void (*)())noprocess);
  de_fault (to_xfer_memory,		(int (*)())nomemory);
  de_fault (to_files_info,		(void (*)())ignore);
  de_fault (to_insert_breakpoint,	memory_insert_breakpoint);
  de_fault (to_remove_breakpoint,	memory_remove_breakpoint);
  de_fault (to_terminal_init,		ignore);
  de_fault (to_terminal_inferior,	ignore);
  de_fault (to_terminal_ours_for_output,ignore);
  de_fault (to_terminal_ours,		ignore);
  de_fault (to_terminal_info,		default_terminal_info);
  de_fault (to_kill,			(void (*)())noprocess);
  de_fault (to_load,			(void (*)())tcomplain);
  de_fault (to_lookup_symbol,		nosymbol);
  de_fault (to_create_inferior,		maybe_kill_then_create_inferior);
  de_fault (to_mourn_inferior,		(void (*)())noprocess);
  de_fault (to_can_run,			return_zero);
  de_fault (to_notice_signals,		(void (*)())ignore);
  de_fault (to_next,			0);
  de_fault (to_has_all_memory,		0);
  de_fault (to_has_memory,		0);
  de_fault (to_has_stack,		0);
  de_fault (to_has_registers,		0);
  de_fault (to_has_execution,		0);

#undef de_fault
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
  struct target_ops *st, *prev;

  for (prev = 0, st = current_target;
       st;
       prev = st, st = st->to_next) {
    if ((int)(t->to_stratum) >= (int)(st->to_stratum))
      break;
  }

  while (t->to_stratum == st->to_stratum) {
    /* There's already something on this stratum.  Close it off.  */
    (st->to_close) (0);
    if (prev)
      prev->to_next = st->to_next;	/* Unchain old target_ops */
    else
      current_target = st->to_next;	/* Unchain first on list */
    st = st->to_next;
  }

  /* We have removed all targets in our stratum, now add ourself.  */
  t->to_next = st;
  if (prev)
    prev->to_next = t;
  else
    current_target = t;

  cleanup_target (current_target);
  return prev != 0;
}

/* Remove a target_ops vector from the stack, wherever it may be. 
   Return how many times it was removed (0 or 1 unless bug).  */

int
unpush_target (t)
     struct target_ops *t;
{
  struct target_ops *u, *v;
  int result = 0;

  for (u = current_target, v = 0;
       u;
       v = u, u = u->to_next)
    if (u == t)
      {
	if (v == 0)
	  pop_target();			/* unchain top copy */
	else {
	  (t->to_close)(0);		/* Let it clean up */
	  v->to_next = t->to_next;	/* unchain middle copy */
	}
	result++;
      }
  return result;
}

void
pop_target ()
{
  (current_target->to_close)(0);	/* Let it clean up */
  current_target = current_target->to_next;
#if 0
  /* This will dump core if ever called--push_target expects current_target
     to be non-NULL.  But I don't think it's needed; I don't see how the
     dummy_target could ever be removed from the stack.  */
  if (!current_target)		/* At bottom, push dummy.  */
    push_target (&dummy_target);
#endif
}

#undef	MIN
#define MIN(A, B) (((A) <= (B)) ? (A) : (B))

/* target_read_string -- read a null terminated string from MEMADDR in target.
   The read may also be terminated early by getting an error from target_xfer_
   memory.
   LEN is the size of the buffer pointed to by MYADDR.  Note that a terminating
   null will only be written if there is sufficient room.  The return value is
   is the number of bytes (including the null) actually transferred.
*/

int
target_read_string (memaddr, myaddr, len)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
{
  int tlen, origlen, offset, i;
  char buf[4];

  origlen = len;

  while (len > 0)
    {
      tlen = MIN (len, 4 - (memaddr & 3));
      offset = memaddr & 3;

      if (target_xfer_memory (memaddr & ~3, buf, 4, 0))
	return origlen - len;

      for (i = 0; i < tlen; i++)
	{
	  *myaddr++ = buf[i + offset];
	  if (buf[i + offset] == '\000')
	    return (origlen - len) + i + 1;
	}

      memaddr += tlen;
      len -= tlen;
    }
  return origlen;
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
  return target_xfer_memory (memaddr, myaddr, len, 0);
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
  errcode = target_xfer_memory (memaddr, myaddr, len, 0);
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
	  errcode = target_xfer_memory (memaddr++, myaddr++, 1, 0);
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
  return target_xfer_memory (memaddr, myaddr, len, 1);
}
 
/* Move memory to or from the targets.  Iterate until all of it has
   been moved, if necessary.  The top target gets priority; anything
   it doesn't want, is offered to the next one down, etc.  Note the
   business with curlen:  if an early target says "no, but I have a
   boundary overlapping this xfer" then we shorten what we offer to
   the subsequent targets so the early guy will get a chance at the
   tail before the subsequent ones do. 

   Result is 0 or errno value.  */

int
target_xfer_memory (memaddr, myaddr, len, write)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
{
  int curlen;
  int res;
  struct target_ops *t;

  /* to_xfer_memory is not guaranteed to set errno, even when it returns
     0.  */
  errno = 0;

  /* The quick case is that the top target does it all.  */
  res = current_target->to_xfer_memory
			(memaddr, myaddr, len, write, current_target);
  if (res == len)
    return 0;

  if (res > 0)
    goto bump;
  /* If res <= 0 then we call it again in the loop.  Ah well.  */

  for (; len > 0;)
    {
      curlen = len;		/* Want to do it all */
      for (t = current_target;
	   t;
	   t = t->to_has_all_memory? 0: t->to_next)
	{
	  res = t->to_xfer_memory(memaddr, myaddr, curlen, write, t);
	  if (res > 0) break;	/* Handled all or part of xfer */
	  if (res == 0) continue;	/* Handled none */
	  curlen = -res;	/* Could handle once we get past res bytes */
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
  int has_all_mem = 0;
  
  if (symfile_objfile != NULL)
    printf ("Symbols from \"%s\".\n", symfile_objfile->name);

#ifdef FILES_INFO_HOOK
  if (FILES_INFO_HOOK ())
    return;
#endif

  for (t = current_target;
       t;
       t = t->to_next)
    {
      if ((int)(t->to_stratum) <= (int)dummy_stratum)
	continue;
      if (has_all_mem)
	printf("\tWhile running this, gdb does not access memory from...\n");
      printf("%s:\n", t->to_longname);
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
  (current_target->to_detach) (args, from_tty);
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
      if (target_can_run(*t))
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

static int
return_zero ()
{
  return 0;
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
      if ((*t)->to_stratum == core_stratum)
	{
	  runable = *t;
	  ++count;
	}
    }
  
  return(count == 1 ? runable : NULL);
}
  
/* Convert a normal process ID to a string.  Returns the string in a static
   buffer.  */

char *
normal_pid_to_str (pid)
     int pid;
{
  static char buf[30];

  sprintf (buf, "process %d", pid);

  return buf;
}

static char targ_desc[] = 
    "Names of targets and files being debugged.\n\
Shows the entire stack of targets currently in use (including the exec-file,\n\
core-file, and process, if any), as well as the symbol file name.";

void
_initialize_targets ()
{
  current_target = &dummy_target;
  cleanup_target (current_target);

  add_info ("target", target_info, targ_desc);
  add_info ("files", target_info, targ_desc);
}
