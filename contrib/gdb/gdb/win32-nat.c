/* Target-vector operations for controlling win32 child processes, for GDB.
   Copyright 1995, 1996
   Free Software Foundation, Inc.

   Contributed by Cygnus Support.
   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without eve nthe implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* by Steve Chamberlain, sac@cygnus.com */

#include "defs.h"
#include "frame.h"		/* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "wait.h"
#include "gdbcore.h"
#include "command.h"
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <windows.h>
#include "buildsym.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb_string.h"
#include "thread.h"
#include "gdbcmd.h"
#include <sys/param.h>

#define CHECK(x) 	check (x, __FILE__,__LINE__)
#define DEBUG_EXEC(x)	if (debug_exec)		printf x
#define DEBUG_EVENTS(x)	if (debug_events)	printf x
#define DEBUG_MEM(x)	if (debug_memory)	printf x
#define DEBUG_EXCEPT(x)	if (debug_exceptions)	printf x

/* Forward declaration */
extern struct target_ops child_ops;

/* The most recently read context. Inspect ContextFlags to see what 
   bits are valid. */

static CONTEXT context;

/* The process and thread handles for the above context. */

static HANDLE current_process;
static HANDLE current_thread;
static int current_process_id;
static int current_thread_id;

/* Counts of things. */
static int exception_count = 0;
static int event_count = 0;

/* User options. */
static int new_console = 0;
static int new_group = 0;
static int dos_path_style = 0;
static int debug_exec = 0;		/* show execution */
static int debug_events = 0;		/* show events from kernel */
static int debug_memory = 0;		/* show target memory accesses */
static int debug_exceptions = 0;	/* show target exceptions */

/* This vector maps GDB's idea of a register's number into an address
   in the win32 exception context vector. 

   It also contains the bit mask needed to load the register in question.  

   One day we could read a reg, we could inspect the context we
   already have loaded, if it doesn't have the bit set that we need,
   we read that set of registers in using GetThreadContext.  If the
   context already contains what we need, we just unpack it. Then to
   write a register, first we have to ensure that the context contains
   the other regs of the group, and then we copy the info in and set
   out bit. */

struct regmappings
  {
    char *incontext;
    int mask;
  };


static const struct regmappings  mappings[] =
{
#ifdef __PPC__
  {(char *) &context.Gpr0, CONTEXT_INTEGER},
  {(char *) &context.Gpr1, CONTEXT_INTEGER},
  {(char *) &context.Gpr2, CONTEXT_INTEGER},
  {(char *) &context.Gpr3, CONTEXT_INTEGER},
  {(char *) &context.Gpr4, CONTEXT_INTEGER},
  {(char *) &context.Gpr5, CONTEXT_INTEGER},
  {(char *) &context.Gpr6, CONTEXT_INTEGER},
  {(char *) &context.Gpr7, CONTEXT_INTEGER},

  {(char *) &context.Gpr8, CONTEXT_INTEGER},
  {(char *) &context.Gpr9, CONTEXT_INTEGER},
  {(char *) &context.Gpr10, CONTEXT_INTEGER},
  {(char *) &context.Gpr11, CONTEXT_INTEGER},
  {(char *) &context.Gpr12, CONTEXT_INTEGER},
  {(char *) &context.Gpr13, CONTEXT_INTEGER},
  {(char *) &context.Gpr14, CONTEXT_INTEGER},
  {(char *) &context.Gpr15, CONTEXT_INTEGER},

  {(char *) &context.Gpr16, CONTEXT_INTEGER},
  {(char *) &context.Gpr17, CONTEXT_INTEGER},
  {(char *) &context.Gpr18, CONTEXT_INTEGER},
  {(char *) &context.Gpr19, CONTEXT_INTEGER},
  {(char *) &context.Gpr20, CONTEXT_INTEGER},
  {(char *) &context.Gpr21, CONTEXT_INTEGER},
  {(char *) &context.Gpr22, CONTEXT_INTEGER},
  {(char *) &context.Gpr23, CONTEXT_INTEGER},

  {(char *) &context.Gpr24, CONTEXT_INTEGER},
  {(char *) &context.Gpr25, CONTEXT_INTEGER},
  {(char *) &context.Gpr26, CONTEXT_INTEGER},
  {(char *) &context.Gpr27, CONTEXT_INTEGER},
  {(char *) &context.Gpr28, CONTEXT_INTEGER},
  {(char *) &context.Gpr29, CONTEXT_INTEGER},
  {(char *) &context.Gpr30, CONTEXT_INTEGER},
  {(char *) &context.Gpr31, CONTEXT_INTEGER},

  {(char *) &context.Fpr0, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr1, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr2, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr3, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr4, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr5, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr6, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr7, CONTEXT_FLOATING_POINT},

  {(char *) &context.Fpr8, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr9, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr10, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr11, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr12, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr13, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr14, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr15, CONTEXT_FLOATING_POINT},

  {(char *) &context.Fpr16, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr17, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr18, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr19, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr20, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr21, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr22, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr23, CONTEXT_FLOATING_POINT},

  {(char *) &context.Fpr24, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr25, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr26, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr27, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr28, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr29, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr30, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr31, CONTEXT_FLOATING_POINT},


  {(char *) &context.Iar, CONTEXT_CONTROL},
  {(char *) &context.Msr, CONTEXT_CONTROL},
  {(char *) &context.Cr,  CONTEXT_INTEGER},
  {(char *) &context.Lr,  CONTEXT_CONTROL},
  {(char *) &context.Ctr, CONTEXT_CONTROL},

  {(char *) &context.Xer, CONTEXT_INTEGER},
  {0,0}, /* MQ, but there isn't one */
#else
  {(char *) &context.Eax, CONTEXT_INTEGER},
  {(char *) &context.Ecx, CONTEXT_INTEGER},
  {(char *) &context.Edx, CONTEXT_INTEGER},
  {(char *) &context.Ebx, CONTEXT_INTEGER},
  {(char *) &context.Esp, CONTEXT_CONTROL},
  {(char *) &context.Ebp, CONTEXT_CONTROL},
  {(char *) &context.Esi, CONTEXT_INTEGER},
  {(char *) &context.Edi, CONTEXT_INTEGER},
  {(char *) &context.Eip, CONTEXT_CONTROL},
  {(char *) &context.EFlags, CONTEXT_CONTROL},
  {(char *) &context.SegCs, CONTEXT_SEGMENTS},
  {(char *) &context.SegSs, CONTEXT_SEGMENTS},
  {(char *) &context.SegDs, CONTEXT_SEGMENTS},
  {(char *) &context.SegEs, CONTEXT_SEGMENTS},
  {(char *) &context.SegFs, CONTEXT_SEGMENTS},
  {(char *) &context.SegGs, CONTEXT_SEGMENTS},
  {&context.FloatSave.RegisterArea[0 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[1 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[2 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[3 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[4 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[5 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[6 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[7 * 10], CONTEXT_FLOATING_POINT},
#endif
};


/* This vector maps the target's idea of an exception (extracted
   from the DEBUG_EVENT structure) to GDB's idea. */

struct xlate_exception
  {
    int them;
    enum target_signal us;
  };


static const struct xlate_exception
  xlate[] =
{
  {EXCEPTION_ACCESS_VIOLATION, TARGET_SIGNAL_SEGV},
  {STATUS_STACK_OVERFLOW, TARGET_SIGNAL_SEGV},
  {EXCEPTION_BREAKPOINT, TARGET_SIGNAL_TRAP},
  {DBG_CONTROL_C, TARGET_SIGNAL_INT},
  {EXCEPTION_SINGLE_STEP, TARGET_SIGNAL_TRAP},
  {-1, -1}};


static void 
check (BOOL ok, const char *file, int line)
{
  if (!ok)
    printf_filtered ("error return %s:%d was %d\n", file, line, GetLastError ());
}

static void
child_fetch_inferior_registers (int r)
{
  if (r < 0)
    {
      for (r = 0; r < NUM_REGS; r++)
	child_fetch_inferior_registers (r);
    }
  else
    {
      supply_register (r, mappings[r].incontext);
    }
}

static void
child_store_inferior_registers (int r)
{
  if (r < 0)
    {
      for (r = 0; r < NUM_REGS; r++)
	child_store_inferior_registers (r);
    }
  else
    {
      read_register_gen (r, mappings[r].incontext);
    }
}


/* Wait for child to do something.  Return pid of child, or -1 in case
   of error; store status through argument pointer OURSTATUS.  */


static int
handle_load_dll (char *eventp)
{
  DEBUG_EVENT * event = (DEBUG_EVENT *)eventp;
  DWORD dll_name_ptr;
  DWORD done;

  ReadProcessMemory (current_process,
		     (DWORD) event->u.LoadDll.lpImageName,
		     (char *) &dll_name_ptr,
		     sizeof (dll_name_ptr), &done);

  /* See if we could read the address of a string, and that the 
     address isn't null. */

  if (done == sizeof (dll_name_ptr) && dll_name_ptr)
    {
      char *dll_name, *dll_basename;
      struct objfile *objfile;
      char unix_dll_name[MAX_PATH];
      int size = event->u.LoadDll.fUnicode ? sizeof (WCHAR) : sizeof (char);
      int len = 0;
      char b[2];
      do
	{
	  ReadProcessMemory (current_process,
			     dll_name_ptr + len * size,
			     &b,
			     size,
			     &done);
	  len++;
	}
      while ((b[0] != 0 || b[size - 1] != 0) && done == size);

      dll_name = alloca (len);

      if (event->u.LoadDll.fUnicode)
	{
	  WCHAR *unicode_dll_name = (WCHAR *) alloca (len * sizeof (WCHAR));
	  ReadProcessMemory (current_process,
			     dll_name_ptr,
			     unicode_dll_name,
			     len * sizeof (WCHAR),
			     &done);

	  WideCharToMultiByte (CP_ACP, 0,
			       unicode_dll_name, len,
			       dll_name, len, 0, 0);
	}
      else
	{
	  ReadProcessMemory (current_process,
			     dll_name_ptr,
			     dll_name,
			     len,
			     &done);
	}


      dos_path_to_unix_path (dll_name, unix_dll_name);

      /* FIXME!! It would be nice to define one symbol which pointed to the 
         front of the dll if we can't find any symbols. */

       if (!(dll_basename = strrchr(dll_name, '\\')))
 	dll_basename = strrchr(dll_name, '/');
 
       ALL_OBJFILES(objfile) 
 	{
 	  char *objfile_basename;
 	  if (!(objfile_basename = strrchr(objfile->name, '\\')))
 	    objfile_basename = strrchr(objfile->name, '/');
 
 	  if (dll_basename && objfile_basename &&
 	      strcmp(dll_basename+1, objfile_basename+1) == 0)
 	    {
 	      printf_unfiltered ("%s (symbols previously loaded)\n", 
 				 dll_basename + 1);
 	      return 1;
 	    }
 	}
 

      context.ContextFlags = CONTEXT_FULL | CONTEXT_FLOATING_POINT;
      GetThreadContext (current_thread, &context);

      /* The symbols in a dll are offset by 0x1000, which is the
	 the offset from 0 of the first byte in an image - because
	 of the file header and the section alignment. 
	 
	 FIXME: Is this the real reason that we need the 0x1000 ? */


      symbol_file_add (unix_dll_name, 0,
		       (int) event->u.LoadDll.lpBaseOfDll + 0x1000, 0, 0, 0);

      printf_unfiltered ("%x:%s\n", event->u.LoadDll.lpBaseOfDll, 
			 unix_dll_name);
    }
  return 1;
}


static void
handle_exception (DEBUG_EVENT * event, struct target_waitstatus *ourstatus)
{
  int i;
  int done = 0;
  ourstatus->kind = TARGET_WAITKIND_STOPPED;


  switch (event->u.Exception.ExceptionRecord.ExceptionCode) 
    {
    case EXCEPTION_ACCESS_VIOLATION:
      DEBUG_EXCEPT (("gdb: Target exception ACCESS_VIOLATION at 0x%08x\n",
		     event->u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_SEGV;
      break;
    case STATUS_STACK_OVERFLOW:
      DEBUG_EXCEPT (("gdb: Target exception STACK_OVERFLOW at 0x%08x\n",
		     event->u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_SEGV;
      break;
    case EXCEPTION_BREAKPOINT:
      DEBUG_EXCEPT (("gdb: Target exception BREAKPOINT at 0x%08x\n",
		     event->u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_TRAP;
      break;
    case DBG_CONTROL_C:
      DEBUG_EXCEPT (("gdb: Target exception CONTROL_C at 0x%08x\n",
		     event->u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_INT;
      break;
    case EXCEPTION_SINGLE_STEP:
      DEBUG_EXCEPT (("gdb: Target exception SINGLE_STEP at 0x%08x\n",
		     event->u.Exception.ExceptionRecord.ExceptionAddress));
      ourstatus->value.sig = TARGET_SIGNAL_TRAP;
      break;
    default:
      printf_unfiltered ("gdb: unknown target exception 0x%08x at 0x%08x\n",
			 event->u.Exception.ExceptionRecord.ExceptionCode,
			 event->u.Exception.ExceptionRecord.ExceptionAddress);
      ourstatus->value.sig = TARGET_SIGNAL_UNKNOWN;
      break;
    }
  context.ContextFlags = CONTEXT_FULL | CONTEXT_FLOATING_POINT;
  GetThreadContext (current_thread, &context);
  exception_count++;
}

static int
child_wait (int pid, struct target_waitstatus *ourstatus)
{
  /* We loop when we get a non-standard exception rather than return
     with a SPURIOUS because resume can try and step or modify things,
     which needs a current_thread.  But some of these exceptions mark
     the birth or death of threads, which mean that the current thread
     isn't necessarily what you think it is. */

  while (1)
    {
      DEBUG_EVENT event;
      BOOL t = WaitForDebugEvent (&event, INFINITE);
      char *p;

      event_count++;

      current_thread_id = event.dwThreadId;
      current_process_id = event.dwProcessId;

      switch (event.dwDebugEventCode)
	{
	case CREATE_THREAD_DEBUG_EVENT:
	  DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n", 
			event.dwProcessId, event.dwThreadId,
			"CREATE_THREAD_DEBUG_EVENT"));
	  break;
	case EXIT_THREAD_DEBUG_EVENT:
	  DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			event.dwProcessId, event.dwThreadId,
			"EXIT_THREAD_DEBUG_EVENT"));
	  break;
	case CREATE_PROCESS_DEBUG_EVENT:
	  DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			event.dwProcessId, event.dwThreadId,
			"CREATE_PROCESS_DEBUG_EVENT"));
	  break;

	case EXIT_PROCESS_DEBUG_EVENT:
	  DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			event.dwProcessId, event.dwThreadId,
			"EXIT_PROCESS_DEBUG_EVENT"));
	  ourstatus->kind = TARGET_WAITKIND_EXITED;
	  ourstatus->value.integer = event.u.ExitProcess.dwExitCode;
	  CloseHandle (current_process);
	  CloseHandle (current_thread);
	  return current_process_id;
	  break;

	case LOAD_DLL_DEBUG_EVENT:
	  DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			event.dwProcessId, event.dwThreadId,
			"LOAD_DLL_DEBUG_EVENT"));
	  catch_errors (handle_load_dll,
			(char*) &event,
			"\n[failed reading symbols from DLL]\n",
			RETURN_MASK_ALL);
	  registers_changed();          /* mark all regs invalid */
	  break;
	case UNLOAD_DLL_DEBUG_EVENT:
	  DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			event.dwProcessId, event.dwThreadId,
			"UNLOAD_DLL_DEBUG_EVENT"));
	  break;	/* FIXME: don't know what to do here */
  	case EXCEPTION_DEBUG_EVENT:
	  DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			event.dwProcessId, event.dwThreadId,
			"EXCEPTION_DEBUG_EVENT"));
	  handle_exception (&event, ourstatus);
	  return current_process_id;

	case OUTPUT_DEBUG_STRING_EVENT: /* message from the kernel */
	  DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			event.dwProcessId, event.dwThreadId,
			"OUTPUT_DEBUG_STRING_EVENT"));
	  if (target_read_string
	      ((CORE_ADDR) event.u.DebugString.lpDebugStringData, 
	       &p, 1024, 0) && p && *p)
	    {
	      warning(p);
	      free(p);
	    }
	  break;
	default:
	  printf_unfiltered ("gdb: kernel event for pid=%d tid=%d\n",
			     event.dwProcessId, event.dwThreadId);
	  printf_unfiltered ("                 unknown event code %d\n",
			     event.dwDebugEventCode);
	  break;
	}
      DEBUG_EVENTS (("ContinueDebugEvent (cpid=%d, ctid=%d, DBG_CONTINUE);\n",
		     current_process_id, current_thread_id));
      CHECK (ContinueDebugEvent (current_process_id,
				 current_thread_id,
				 DBG_CONTINUE));
    }
}


/* Attach to process PID, then initialize for debugging it.  */

static void
child_attach (args, from_tty)
     char *args;
     int from_tty;
{
  BOOL ok;

  if (!args)
    error_no_arg ("process-id to attach");

  current_process_id = strtoul (args, 0, 0);

  ok = DebugActiveProcess (current_process_id);

  if (!ok)
    error ("Can't attach to process.");


  exception_count = 0;
  event_count = 0;

  if (from_tty)
    {
      char *exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf_unfiltered ("Attaching to program `%s', %s\n", exec_file,
			   target_pid_to_str (current_process_id));
      else
	printf_unfiltered ("Attaching to %s\n",
			   target_pid_to_str (current_process_id));

      gdb_flush (gdb_stdout);
    }

  inferior_pid = current_process_id;
  push_target (&child_ops);
}


static void
child_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf_unfiltered ("Detaching from program: %s %s\n", exec_file,
			 target_pid_to_str (inferior_pid));
      gdb_flush (gdb_stdout);
    }
  inferior_pid = 0;
  unpush_target (&child_ops);
}


/* Print status information about what we're accessing.  */

static void
child_files_info (ignore)
     struct target_ops *ignore;
{
  printf_unfiltered ("\tUsing the running image of %s %s.\n",
      attach_flag ? "attached" : "child", target_pid_to_str (inferior_pid));
}

/* ARGSUSED */
static void
child_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  error ("Use the \"run\" command to start a Unix child process.");
}


/* Convert a unix-style set-of-paths (a colon-separated list of directory
   paths with forward slashes) into the dos style (semicolon-separated 
   list with backward slashes), simultaneously undoing any translations
   performed by the mount table. */

static char *buf = NULL;
static int blen = 2000;

static char *
unix_paths_to_dos_paths(char *newenv)
{
  int ei;
  char *src;

  if (buf == 0)
    buf = (char *) malloc(blen);

  if (newenv == 0 || *newenv == 0 ||
     (src = strchr(newenv, '=')) == 0)	/* find the equals sign */
    return 0;

  src++;				/* now skip past it */

  if (src[0] == '/' ||			/* is this a unix style path? */
     (src[0] == '.' && src[1] == '/') ||
     (src[0] == '.' && src[1] == '.' && src[2] == '/'))
    { /* we accept that we will fail on a relative path like 'foo/mumble' */
      /* Found an env name, turn from unix style into dos style */
      int len = src - newenv;
      char *dir = buf + len;

      memcpy(buf, newenv, len);
      /* Split out the colons */
      while (1)
	{
	  char *tok = strchr (src, ':');
	  int doff = dir - buf;

	  if (doff + MAX_PATH > blen) 
	    {
	      blen *= 2;
	      buf = (char *) realloc((void *) buf, blen);
	      dir = buf + doff;
	    }
	  if (tok)
	    {
	      *tok = 0;
	            cygwin32_unix_path_to_dos_path_keep_rel (src, dir);
	      *tok = ':';
	      dir += strlen(dir);
	      src = tok + 1;
	      *dir++ = ';';
	    }
	  else
	    {
      cygwin32_unix_path_to_dos_path_keep_rel (src, dir);
	      dir += strlen(dir);
	      *dir++ = 0;
	      break;
	    }
	}
      return buf;
    }
  return 0;
}

/* Convert a dos-style set-of-paths (a semicolon-separated list with
   backward slashes) into the dos style (colon-separated list of
   directory paths with forward slashes), simultaneously undoing any
   translations performed by the mount table. */

static char *
dos_paths_to_unix_paths(char *newenv)
{
  int ei;
  char *src;

  if (buf == 0)
    buf = (char *) malloc(blen);

  if (newenv == 0 || *newenv == 0 ||
     (src = strchr(newenv, '=')) == 0)	/* find the equals sign */
    return 0;

  src++;				/* now skip past it */

  if (src[0] == '\\' ||		/* is this a dos style path? */
     (isalpha(src[0]) && src[1] == ':' && src[2] == '\\') ||
     (src[0] == '.' && src[1] == '\\') ||
     (src[0] == '.' && src[1] == '.' && src[2] == '\\'))
    { /* we accept that we will fail on a relative path like 'foo\mumble' */
      /* Found an env name, turn from dos style into unix style */
      int len = src - newenv;
      char *dir = buf + len;

      memcpy(buf, newenv, len);
      /* Split out the colons */
      while (1)
	{
	  char *tok = strchr (src, ';');
	  int doff = dir - buf;
	  
	  if (doff + MAX_PATH > blen) 
	    {
	      blen *= 2;
	      buf = (char *) realloc((void *) buf, blen);
	      dir = buf + doff;
	    }
	  if (tok)
	    {
	      *tok = 0;
	         cygwin32_dos_path_to_unix_path_keep_rel (src, dir);
	      *tok = ';';
	      dir += strlen(dir);
	      src = tok + 1;
	      *dir++ = ':';
	    }
	  else
	    {
	         cygwin32_dos_path_to_unix_path_keep_rel (src, dir);
	      dir += strlen(dir);
	      *dir++ = 0;
	      break;
	    }
	}
      return buf;
    }
  return 0;
}


/* Start an inferior win32 child process and sets inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().  */

static void
child_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  char real_path[MAXPATHLEN];
  char *winenv;
  char *temp;
  int  envlen;
  int i;

  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  struct target_waitstatus dummy;
  BOOL ret;
  DWORD flags;
  char *args;

  if (!exec_file)
    {
      error ("No executable specified, use `target exec'.\n");
    }

  memset (&si, 0, sizeof (si));
  si.cb = sizeof (si);

  unix_path_to_dos_path (exec_file, real_path);

  flags = DEBUG_ONLY_THIS_PROCESS; 

  if (new_group)
    flags |= CREATE_NEW_PROCESS_GROUP;

  if (new_console)
    flags |= CREATE_NEW_CONSOLE;

  args = alloca (strlen (real_path) + strlen (allargs) + 2);

  strcpy (args, real_path);

  strcat (args, " ");
  strcat (args, allargs);

#if 0
  /* get total size for env strings */
  for (envlen = 0, i = 0; env[i] && *env[i]; i++)
    envlen += strlen(env[i]) + 1;       
#else
  /* get total size for env strings */
  for (envlen = 0, i = 0; env[i] && *env[i]; i++)
    {
#if 0
      winenv = 0;
#else
      winenv = unix_paths_to_dos_paths(env[i]);
#endif
      envlen += winenv ? strlen(winenv) + 1 : strlen(env[i]) + 1;
    }
#endif

  winenv = alloca(2 * envlen + 1);	/* allocate new buffer */

  /* copy env strings into new buffer */
  for (temp = winenv, i = 0; env[i] && *env[i];     i++) 
    {
#if 0
      char *p = 0;
#else
      char *p = unix_paths_to_dos_paths(env[i]);
#endif
      strcpy(temp, p ? p : env[i]);
      temp += strlen(temp) + 1;
    }
#if 0
  /* copy env strings into new buffer */
  for (temp = winenv, i = 0; env[i] && *env[i];     i++) 
    {
      strcpy(temp, env[i]);
      temp += strlen(temp) + 1;
    }
#endif

  *temp = 0;			/* final nil string to terminate new env */

  ret = CreateProcess (0,
		       args, 	/* command line */
		       NULL,	/* Security */
		       NULL,	/* thread */
		       TRUE,	/* inherit handles */
		       flags,	/* start flags */
		       winenv,
		       NULL,	/* current directory */
		       &si,
		       &pi);
  if (!ret)
    error ("Error creating process %s, (error %d)\n", exec_file, GetLastError());

  exception_count = 0;
  event_count = 0;

  inferior_pid = pi.dwProcessId;
  current_process = pi.hProcess;
  current_thread = pi.hThread;
  current_process_id = pi.dwProcessId;
  current_thread_id = pi.dwThreadId;
  push_target (&child_ops);
  init_thread_list ();
  init_wait_for_inferior ();
  clear_proceed_status ();
  target_terminal_init ();
  target_terminal_inferior ();

  /* Ignore the first trap */
  child_wait (inferior_pid, &dummy);

  proceed ((CORE_ADDR) - 1, TARGET_SIGNAL_0, 0);
}

static void
child_mourn_inferior ()
{
  unpush_target (&child_ops);
  generic_mourn_inferior ();
}


/* Send a SIGINT to the process group.  This acts just like the user typed a
   ^C on the controlling terminal. */

void
child_stop ()
{
  DEBUG_EVENTS (("gdb: GenerateConsoleCtrlEvent (CTRLC_EVENT, 0)\n"));
  CHECK (GenerateConsoleCtrlEvent (CTRL_C_EVENT, 0));
  registers_changed();		/* refresh register state */
}

int
child_xfer_memory (CORE_ADDR memaddr, char *our, int len,
		   int write, struct target_ops *target)
{
  DWORD done;
  if (write)
    {
      DEBUG_MEM (("gdb: write target memory, %d bytes at 0x%08x\n",
		  len, memaddr));
      WriteProcessMemory (current_process, memaddr, our, len, &done);
      FlushInstructionCache (current_process, memaddr, len);
    }
  else
    {
      DEBUG_MEM (("gdb: read target memory, %d bytes at 0x%08x\n",
		  len, memaddr));
      ReadProcessMemory (current_process, memaddr, our, len, &done);
    }
  return done;
}

void
child_kill_inferior (void)
{
  CHECK (TerminateProcess (current_process, 0));
  CHECK (CloseHandle (current_process));
  CHECK (CloseHandle (current_thread));
  target_mourn_inferior();	/* or just child_mourn_inferior? */
}

void
child_resume (int pid, int step, enum target_signal signal)
{
  DEBUG_EXEC (("gdb: child_resume (pid=%d, step=%d, signal=%d);\n", 
	       pid, step, signal));

  if (step)
    {
#ifdef __PPC__
      warning ("Single stepping not done.\n");
#endif
#ifdef i386
      /* Single step by setting t bit */
      child_fetch_inferior_registers (PS_REGNUM);
      context.EFlags |= FLAG_TRACE_BIT;
#endif
    }

  if (context.ContextFlags)
    {
      CHECK (SetThreadContext (current_thread, &context));
      context.ContextFlags = 0;
    }

  if (signal)
    {
      fprintf_unfiltered (gdb_stderr, "Can't send signals to the child.\n");
    }

  DEBUG_EVENTS (("gdb: ContinueDebugEvent (cpid=%d, ctid=%d, DBG_CONTINUE);\n",
		 current_process_id, current_thread_id));
  CHECK (ContinueDebugEvent (current_process_id,
			     current_thread_id,
			     DBG_CONTINUE));
}

static void
child_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static int
child_can_run ()
{
  return 1;
}

static void
child_close ()
{
  DEBUG_EVENTS (("gdb: child_close, inferior_pid=%d\n", inferior_pid));
}

struct target_ops child_ops =
{
  "child",			/* to_shortname */
  "Win32 child process",	/* to_longname */
  "Win32 child process (started by the \"run\" command).",	/* to_doc */
  child_open,			/* to_open */
  child_close,			/* to_close */
  child_attach,			/* to_attach */
  child_detach,			/* to_detach */
  child_resume,			/* to_resume */
  child_wait,			/* to_wait */
  child_fetch_inferior_registers,/* to_fetch_registers */
  child_store_inferior_registers,/* to_store_registers */
  child_prepare_to_store,	/* to_child_prepare_to_store */
  child_xfer_memory,		/* to_xfer_memory */
  child_files_info,		/* to_files_info */
  memory_insert_breakpoint,	/* to_insert_breakpoint */
  memory_remove_breakpoint,	/* to_remove_breakpoint */
  terminal_init_inferior,	/* to_terminal_init */
  terminal_inferior,		/* to_terminal_inferior */
  terminal_ours_for_output,	/* to_terminal_ours_for_output */
  terminal_ours,		/* to_terminal_ours */
  child_terminal_info,		/* to_terminal_info */
  child_kill_inferior,		/* to_kill */
  0,				/* to_load */
  0,				/* to_lookup_symbol */
  child_create_inferior,	/* to_create_inferior */
  child_mourn_inferior,		/* to_mourn_inferior */
  child_can_run,		/* to_can_run */
  0,				/* to_notice_signals */
  0,				/* to_thread_alive */
  child_stop,			/* to_stop */
  process_stratum,		/* to_stratum */
  0,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  0,				/* to_sections */
  0,				/* to_sections_end */
  OPS_MAGIC			/* to_magic */
};

#include "environ.h"

static void
set_pathstyle_dos(args, from_tty, c)
     char *args;
     int from_tty;
     struct cmd_list_element *c;
{
  char **vector = environ_vector(inferior_environ);
  char *thisvar;
  int dos = *(int *) c->var;

  if (info_verbose)
    printf_unfiltered ("Change dos_path_style to %s\n", dos ? "true":"false");

  while (vector && *vector) 
    {
      if (dos)
	thisvar = unix_paths_to_dos_paths(*vector);
      else
	thisvar = dos_paths_to_unix_paths(*vector);

      if (thisvar) 
	{
	  if (info_verbose)
	    printf_unfiltered ("Change %s\nto %s\n", *vector, thisvar);
	  free(*vector);
	  *vector = xmalloc(strlen(thisvar) + 1);
	  strcpy(*vector, thisvar);
	}
      vector++;
    }
}


void
_initialize_inftarg ()
{
  struct cmd_list_element *c;

  add_show_from_set
    (add_set_cmd ("new-console", class_support, var_boolean,
		  (char *) &new_console,
		  "Set creation of new console when creating child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("new-group", class_support, var_boolean,
		  (char *) &new_group,
		  "Set creation of new group when creating child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (c = add_set_cmd ("dos-path-style", class_support, var_boolean,
		      (char *) &dos_path_style,
		      "Set whether paths in child's environment are shown in dos style.",
		  &setlist),
     &showlist);

  c->function.sfunc = set_pathstyle_dos;

  add_show_from_set
    (add_set_cmd ("debugexec", class_support, var_boolean,
		  (char *) &debug_exec,
		  "Set whether to display execution in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugevents", class_support, var_boolean,
		  (char *) &debug_events,
		  "Set whether to display kernel events in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugmemory", class_support, var_boolean,
		  (char *) &debug_memory,
		  "Set whether to display memory accesses in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugexceptions", class_support, var_boolean,
		  (char *) &debug_exceptions,
		  "Set whether to display kernel exceptions in child process.",
		  &setlist),
     &showlist);

  add_target (&child_ops);
}
