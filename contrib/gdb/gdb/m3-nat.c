/* Interface GDB to Mach 3.0 operating systems.
   (Most) Mach 3.0 related routines live in this file.

   Copyright (C) 1992 Free Software Foundation, Inc.

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

/*
 * Author: Jukka Virtanen <jtv@hut.fi>
 *	   Computing Centre
 *         Helsinki University of Technology
 *         Finland
 *
 * Thanks to my friends who helped with ideas and testing:
 *
 *	Johannes Helander, Antti Louko, Tero Mononen,
 *	jvh@cs.hut.fi	   alo@hut.fi   tmo@cs.hut.fi
 *
 *      Tero Kivinen       and          Eamonn McManus
 *	kivinen@cs.hut.fi               emcmanus@gr.osf.org
 *	
 */

#include <stdio.h>

#include <mach.h>
#include <servers/netname.h>
#include <servers/machid.h>
#include <mach/message.h>
#include <mach/notify.h>
#include <mach_error.h>
#include <mach/exception.h>
#include <mach/vm_attributes.h>

#include "defs.h"
#include "inferior.h"
#include "symtab.h"
#include "value.h"
#include "language.h"
#include "target.h"
#include "wait.h"
#include "gdbcmd.h"
#include "gdbcore.h"

#if 0
#include <servers/machid_lib.h>
#else
#define	MACH_TYPE_TASK			1
#define MACH_TYPE_THREAD		2
#endif

/* Included only for signal names and NSIG
 *
 * note: There are many problems in signal handling with
 *       gdb in Mach 3.0 in general.
 */
#include <signal.h>
#define SIG_UNKNOWN 0	/* Exception that has no matching unix signal */

#include <cthreads.h>

/* This is what a cproc looks like.  This is here partly because
   cthread_internals.h is not a header we can just #include, partly with
   an eye towards perhaps getting this to work with cross-debugging
   someday.  Best solution is if CMU publishes a real interface to this
   stuff.  */
#define CPROC_NEXT_OFFSET 0
#define CPROC_NEXT_SIZE (TARGET_PTR_BIT / HOST_CHAR_BIT)
#define CPROC_INCARNATION_OFFSET (CPROC_NEXT_OFFSET + CPROC_NEXT_SIZE)
#define CPROC_INCARNATION_SIZE (sizeof (cthread_t))
#define CPROC_LIST_OFFSET (CPROC_INCARNATION_OFFSET + CPROC_INCARNATION_SIZE)
#define CPROC_LIST_SIZE (TARGET_PTR_BIT / HOST_CHAR_BIT)
#define CPROC_WAIT_OFFSET (CPROC_LIST_OFFSET + CPROC_LIST_SIZE)
#define CPROC_WAIT_SIZE (TARGET_PTR_BIT / HOST_CHAR_BIT)
#define CPROC_REPLY_OFFSET (CPROC_WAIT_OFFSET + CPROC_WAIT_SIZE)
#define CPROC_REPLY_SIZE (sizeof (mach_port_t))
#define CPROC_CONTEXT_OFFSET (CPROC_REPLY_OFFSET + CPROC_REPLY_SIZE)
#define CPROC_CONTEXT_SIZE (TARGET_INT_BIT / HOST_CHAR_BIT)
#define CPROC_LOCK_OFFSET (CPROC_CONTEXT_OFFSET + CPROC_CONTEXT_SIZE)
#define CPROC_LOCK_SIZE (sizeof (spin_lock_t))
#define CPROC_STATE_OFFSET (CPROC_LOCK_OFFSET + CPROC_LOCK_SIZE)
#define CPROC_STATE_SIZE (TARGET_INT_BIT / HOST_CHAR_BIT)
#define CPROC_WIRED_OFFSET (CPROC_STATE_OFFSET + CPROC_STATE_SIZE)
#define CPROC_WIRED_SIZE (sizeof (mach_port_t))
#define CPROC_BUSY_OFFSET (CPROC_WIRED_OFFSET + CPROC_WIRED_SIZE)
#define CPROC_BUSY_SIZE (TARGET_INT_BIT / HOST_CHAR_BIT)
#define CPROC_MSG_OFFSET (CPROC_BUSY_OFFSET + CPROC_BUSY_SIZE)
#define CPROC_MSG_SIZE (sizeof (mach_msg_header_t))
#define CPROC_BASE_OFFSET (CPROC_MSG_OFFSET + CPROC_MSG_SIZE)
#define CPROC_BASE_SIZE (TARGET_INT_BIT / HOST_CHAR_BIT)
#define CPROC_SIZE_OFFSET (CPROC_BASE_OFFSET + CPROC_BASE_SIZE)
#define CPROC_SIZE_SIZE (TARGET_INT_BIT / HOST_CHAR_BIT)
#define CPROC_SIZE (CPROC_SIZE_OFFSET + CPROC_SIZE_SIZE)

/* Values for the state field in the cproc.  */
#define CPROC_RUNNING	0
#define CPROC_SWITCHING 1
#define CPROC_BLOCKED	2
#define CPROC_CONDWAIT	4

/* For cproc and kernel thread mapping */
typedef struct gdb_thread {
  mach_port_t	name;
  CORE_ADDR	sp;
  CORE_ADDR	pc;
  CORE_ADDR	fp;
  boolean_t     in_emulator;
  int		slotid;

  /* This is for the mthreads list.  It points to the cproc list.
     Perhaps the two lists should be merged (or perhaps it was a mistake
     to make them both use a struct gdb_thread).  */
  struct gdb_thread *cproc;

  /* These are for the cproc list, which is linked through the next field
     of the struct gdb_thread.  */
  char raw_cproc[CPROC_SIZE];
  /* The cthread which is pointed to by the incarnation field from the
     cproc.  This points to the copy we've read into GDB.  */
  cthread_t cthread;
  /* Point back to the mthreads list.  */
  int reverse_map;
  struct gdb_thread *next;
} *gdb_thread_t;

/* 
 * Actions for Mach exceptions.
 *
 * sigmap field maps the exception to corresponding Unix signal.
 *
 * I do not know how to map the exception to unix signal
 * if SIG_UNKNOWN is specified.
 */

struct exception_list {
  char *name;
  boolean_t forward;
  boolean_t print;
  int       sigmap;
} exception_map[] = {
  {"not_mach3_exception",	FALSE, TRUE,  SIG_UNKNOWN},
  {"EXC_BAD_ACCESS",		FALSE, TRUE,  SIGSEGV},
  {"EXC_BAD_INSTRUCTION",	FALSE, TRUE,  SIGILL},
  {"EXC_ARITHMETIC",		FALSE, TRUE,  SIGFPE},
  {"EXC_EMULATION",		FALSE, TRUE,  SIGEMT},	/* ??? */
  {"EXC_SOFTWARE",		FALSE, TRUE,  SIG_UNKNOWN},
  {"EXC_BREAKPOINT",		FALSE, FALSE, SIGTRAP}
};

/* Mach exception table size */
int max_exception = sizeof(exception_map)/sizeof(struct exception_list) - 1;

#define MAX_EXCEPTION max_exception

WAITTYPE wait_status;

/* If you define this, intercepted bsd server calls will be
 * dumped while waiting the inferior to EXEC the correct
 * program
 */
/* #define DUMP_SYSCALL		/* debugging interceptor */

/* xx_debug() outputs messages if this is nonzero.
 * If > 1, DUMP_SYSCALL will dump message contents.
 */
int debug_level = 0;

/* "Temporary" debug stuff */
void
xx_debug (fmt, a,b,c)
char *fmt;
int a,b,c;
{
  if (debug_level)
    warning (fmt, a, b, c);
}

/* This is in libmach.a */
extern  mach_port_t  name_server_port;

/* Set in catch_exception_raise */
int stop_exception, stop_code, stop_subcode;
int stopped_in_exception;

/* Thread that was the active thread when we stopped */
thread_t stop_thread = MACH_PORT_NULL;

char *hostname = "";

/* Set when task is attached or created */
boolean_t emulator_present = FALSE;

task_t   inferior_task;
thread_t current_thread;

/* Exception ports for inferior task */
mach_port_t inferior_exception_port     = MACH_PORT_NULL;
mach_port_t inferior_old_exception_port = MACH_PORT_NULL;

/* task exceptions and notifications */
mach_port_t inferior_wait_port_set	= MACH_PORT_NULL;
mach_port_t our_notify_port       	= MACH_PORT_NULL;

/* This is "inferior_wait_port_set" when not single stepping, and
 *         "singlestepped_thread_port" when we are single stepping.
 * 
 * This is protected by a cleanup function: discard_single_step()
 */
mach_port_t currently_waiting_for	= MACH_PORT_NULL;

/* A port for external messages to gdb.
 * External in the meaning that they do not come
 * from the inferior_task, but rather from external
 * tasks.
 *
 * As a debugging feature:
 * A debugger debugging another debugger can stop the
 * inferior debugger by the following command sequence
 * (without running external programs)
 *
 *    (top-gdb) set stop_inferior_gdb ()
 *    (top-gdb) continue
 */
mach_port_t our_message_port       	= MACH_PORT_NULL;

/* For single stepping */
mach_port_t thread_exception_port 	= MACH_PORT_NULL;
mach_port_t thread_saved_exception_port = MACH_PORT_NULL;
mach_port_t singlestepped_thread_port   = MACH_PORT_NULL;

/* For machid calls */
mach_port_t mid_server = MACH_PORT_NULL;
mach_port_t mid_auth   = MACH_PORT_NULL;

/* If gdb thinks the inferior task is not suspended, it
 * must take suspend/abort the threads when it reads the state.
 */
int must_suspend_thread = 0;

/* When single stepping, we switch the port that mach_really_wait() listens to.
 * This cleanup is a guard to prevent the port set from being left to
 * the singlestepped_thread_port when error() is called.
 *  This is nonzero only when we are single stepping.
 */
#define NULL_CLEANUP (struct cleanup *)0
struct cleanup *cleanup_step = NULL_CLEANUP;


extern struct target_ops m3_ops;
static void m3_kill_inferior ();

#if 0
#define MACH_TYPE_EXCEPTION_PORT	-1
#endif

/* Chain of ports to remember requested notifications. */

struct port_chain {
  struct port_chain *next;
  mach_port_t	     port;
  int		     type;
  int		     mid;  /* Now only valid with MACH_TYPE_THREAD and */
  			   /*  MACH_TYPE_THREAD */
};
typedef struct port_chain *port_chain_t;

/* Room for chain nodes comes from pchain_obstack */
struct obstack pchain_obstack;
struct obstack *port_chain_obstack = &pchain_obstack;

/* For thread handling */
struct obstack Cproc_obstack;
struct obstack *cproc_obstack = &Cproc_obstack;

/* the list of notified ports */
port_chain_t notify_chain = (port_chain_t) NULL;

port_chain_t
port_chain_insert (list, name, type)
     port_chain_t list;
     mach_port_t name;
     int	 type;
{
  kern_return_t ret;
  port_chain_t new;
  int mid;

  if (! MACH_PORT_VALID (name))
    return list;
  
  if (type == MACH_TYPE_TASK || type == MACH_TYPE_THREAD)
    {
      if (! MACH_PORT_VALID (mid_server))
	{
	  warning ("Machid server port invalid, can not map port 0x%x to MID",
		   name);
	  mid = name;
	}
      else
	{
	  ret = machid_mach_register (mid_server, mid_auth, name, type, &mid);
	  
	  if (ret != KERN_SUCCESS)
	    {
	      warning ("Can not map name (0x%x) to MID with machid", name);
	      mid = name;
	    }
	}
    }
  else
    abort ();

  new = (port_chain_t) obstack_alloc (port_chain_obstack,
				      sizeof (struct port_chain));
  new->next  = list;
  new->port  = name;
  new->type  = type;
  new->mid   = mid;

  return new;
}

port_chain_t
port_chain_delete (list, elem)
     port_chain_t list;
     mach_port_t elem;
{
  if (list)
    if (list->port == elem)
      list = list->next;
    else
      while (list->next)
	{
	  if (list->next->port == elem)
	    list->next = list->next->next; /* GCd with obstack_free() */
	  else
	    list = list->next;
	}
  return list;
}

void
port_chain_destroy (ostack)
     struct obstack *ostack;
{
  obstack_free (ostack, 0);
  obstack_init (ostack);
}

port_chain_t
port_chain_member (list, elem)
     port_chain_t list;
     mach_port_t elem;
{
  while (list)
    {
      if (list->port == elem)
	return list;
      list = list->next;
    }
  return (port_chain_t) NULL;
}

int
map_port_name_to_mid (name, type)
mach_port_t name;
int         type;
{
  port_chain_t elem;

  if (!MACH_PORT_VALID (name))
    return -1;

  elem = port_chain_member (notify_chain, name);

  if (elem && (elem->type == type))
    return elem->mid;
  
  if (elem)
    return -1;
  
  if (! MACH_PORT_VALID (mid_server))
    {
      warning ("Machid server port invalid, can not map port 0x%x to mid",
	       name);
      return -1;
    }
  else
    {
      int mid;
      kern_return_t ret;

      ret = machid_mach_register (mid_server, mid_auth, name, type, &mid);
      
      if (ret != KERN_SUCCESS)
	{
	  warning ("Can not map name (0x%x) to mid with machid", name);
	  return -1;
	}
      return mid;
    }
}

/* Guard for currently_waiting_for and singlestepped_thread_port */
static void
discard_single_step (thread)
     thread_t thread;
{
  currently_waiting_for = inferior_wait_port_set;

  cleanup_step = NULL_CLEANUP;
  if (MACH_PORT_VALID (thread) && MACH_PORT_VALID (singlestepped_thread_port))
    setup_single_step (thread, FALSE);
}

setup_single_step (thread, start_step)
     thread_t  thread;
     boolean_t start_step;
{
  kern_return_t ret;

  if (! MACH_PORT_VALID (thread))
    error ("Invalid thread supplied to setup_single_step");
  else
    {
      mach_port_t teport;

      /* Get the current thread exception port */
      ret = thread_get_exception_port (thread, &teport);
      CHK ("Getting thread's exception port", ret);
	  
      if (start_step)
	{
	  if (MACH_PORT_VALID (singlestepped_thread_port))
	    {
	      warning ("Singlestepped_thread_port (0x%x) is still valid?",
		       singlestepped_thread_port);
	      singlestepped_thread_port = MACH_PORT_NULL;
	    }
      
	  /* If we are already stepping this thread */
	  if (MACH_PORT_VALID (teport) && teport == thread_exception_port)
	    {
	      ret = mach_port_deallocate (mach_task_self (), teport);
	      CHK ("Could not deallocate thread exception port", ret);
	    }
	  else
	    {
	      ret = thread_set_exception_port (thread, thread_exception_port);
	      CHK ("Setting exception port for thread", ret);
#if 0
	      /* Insert thread exception port to wait port set */
	      ret = mach_port_move_member (mach_task_self(), 
					   thread_exception_port,
					   inferior_wait_port_set);
	      CHK ("Moving thread exception port to inferior_wait_port_set",
		   ret);
#endif
	      thread_saved_exception_port = teport;
	    }
	  
	  thread_trace (thread, TRUE);
	  
	  singlestepped_thread_port   = thread_exception_port;
	  currently_waiting_for       = singlestepped_thread_port;
	  cleanup_step = make_cleanup (discard_single_step, thread);
	}
      else
	{
	  if (! MACH_PORT_VALID (teport))
	    error ("Single stepped thread had an invalid exception port?");

	  if (teport != thread_exception_port)
	    error ("Single stepped thread had an unknown exception port?");
	  
	  ret = mach_port_deallocate (mach_task_self (), teport);
	  CHK ("Couldn't deallocate thread exception port", ret);
#if 0
	  /* Remove thread exception port from wait port set */
	  ret = mach_port_move_member (mach_task_self(), 
				       thread_exception_port,
				       MACH_PORT_NULL);
	  CHK ("Removing thread exception port from inferior_wait_port_set",
	       ret);
#endif	  
	  /* Restore thread's old exception port */
	  ret = thread_set_exception_port (thread,
					   thread_saved_exception_port);
	  CHK ("Restoring stepped thread's exception port", ret);
	  
	  if (MACH_PORT_VALID (thread_saved_exception_port))
	    (void) mach_port_deallocate (mach_task_self (),
					 thread_saved_exception_port);
	  
	  thread_trace (thread, FALSE);
	  
	  singlestepped_thread_port = MACH_PORT_NULL;
	  currently_waiting_for = inferior_wait_port_set;
	  if (cleanup_step)
	    discard_cleanups (cleanup_step);
	}
    }
}

static
request_notify (name, variant, type)
     mach_port_t	name;
     mach_msg_id_t	variant;
     int	        type;
{
  kern_return_t ret;
  mach_port_t	previous_port_dummy = MACH_PORT_NULL;
  
  if (! MACH_PORT_VALID (name))
    return;
  
  if (port_chain_member (notify_chain, name))
    return;

  ret = mach_port_request_notification (mach_task_self(),
					name,
					variant,
					1,
					our_notify_port,
					MACH_MSG_TYPE_MAKE_SEND_ONCE,
					&previous_port_dummy);
  CHK ("Serious: request_notify failed", ret);

  (void) mach_port_deallocate (mach_task_self (),
			       previous_port_dummy);

  notify_chain = port_chain_insert (notify_chain, name, type);
}

reverse_msg_bits(msgp, type)
     mach_msg_header_t	*msgp;
     int type;
{
  int		rbits,lbits;
  rbits = MACH_MSGH_BITS_REMOTE(msgp->msgh_bits);
  lbits = type;
  msgp->msgh_bits =
    (msgp->msgh_bits & ~MACH_MSGH_BITS_PORTS_MASK) |
      MACH_MSGH_BITS(lbits,rbits);
}

/* On the third day He said:

   	Let this be global
	and then it was global.

   When creating the inferior fork, the
   child code in inflow.c sets the name of the
   bootstrap_port in its address space to this
   variable.

   The name is transferred to our address space
   with mach3_read_inferior().

   Thou shalt not do this with
   task_get_bootstrap_port() in this task, since
   the name in the inferior task is different than
   the one we get.

   For blessed are the meek, as they shall inherit
   the address space.
 */
mach_port_t original_server_port_name = MACH_PORT_NULL;


/* Called from inferior after FORK but before EXEC */
static void
m3_trace_me ()
{
  kern_return_t ret;
  
  /* Get the NAME of the bootstrap port in this task
     so that GDB can read it */
  ret = task_get_bootstrap_port (mach_task_self (),
				 &original_server_port_name);
  if (ret != KERN_SUCCESS)
    abort ();
  ret = mach_port_deallocate (mach_task_self (),
			      original_server_port_name);
  if (ret != KERN_SUCCESS)
    abort ();
  
  /* Suspend this task to let the parent change my ports.
     Resumed by the debugger */
  ret = task_suspend (mach_task_self ());
  if (ret != KERN_SUCCESS)
    abort ();
}

/*
 * Intercept system calls to Unix server.
 * After EXEC_COUNTER calls to exec(), return.
 *
 * Pre-assertion:  Child is suspended. (Not verified)
 * Post-condition: Child is suspended after EXEC_COUNTER exec() calls.
 */

void
intercept_exec_calls (exec_counter)
     int exec_counter;
{
  int terminal_initted = 0;

  struct syscall_msg_t {
    mach_msg_header_t	header;
    mach_msg_type_t 	type;
    char room[ 2000 ];	/* Enuff space */
  };

  struct syscall_msg_t syscall_in, syscall_out;

  mach_port_t fake_server;
  mach_port_t original_server_send;
  mach_port_t original_exec_reply;
  mach_port_t exec_reply;
  mach_port_t exec_reply_send;
  mach_msg_type_name_t acquired;
  mach_port_t emulator_server_port_name;
  struct task_basic_info info;
  mach_msg_type_number_t info_count;

  kern_return_t ret;

  if (exec_counter <= 0)
    return;		/* We are already set up in the correct program */

  ret = mach_port_allocate(mach_task_self(), 
			   MACH_PORT_RIGHT_RECEIVE,
			   &fake_server);
  CHK("create inferior_fake_server port failed", ret);
  
  /* Wait for inferior_task to suspend itself */
  while(1)
    {
      info_count = sizeof (info);
      ret = task_info (inferior_task,
		       TASK_BASIC_INFO,
		       (task_info_t)&info,
		       &info_count);
      CHK ("Task info", ret);

      if (info.suspend_count)
	break;

      /* Note that the definition of the parameter was undefined
       * at the time of this writing, so I just use an `ad hoc' value.
       */
      (void) swtch_pri (42); /* Universal Priority Value */
    }

  /* Read the inferior's bootstrap port name */
  if (!mach3_read_inferior (&original_server_port_name,
			    &original_server_port_name,
			    sizeof (original_server_port_name)))
    error ("Can't read inferior task bootstrap port name");

  /* @@ BUG: If more than 1 send right GDB will FAIL!!! */
  /*      Should get refs, and set them back when restoring */
  /* Steal the original bsd server send right from inferior */
  ret = mach_port_extract_right (inferior_task,
				 original_server_port_name,
				 MACH_MSG_TYPE_MOVE_SEND,
				 &original_server_send,
				 &acquired);
  CHK("mach_port_extract_right (bsd server send)",ret);
  
  if (acquired != MACH_MSG_TYPE_PORT_SEND)
    error("Incorrect right extracted, send right to bsd server excpected");

  ret = mach_port_insert_right (inferior_task,
				original_server_port_name,
				fake_server,
				MACH_MSG_TYPE_MAKE_SEND);
  CHK("mach_port_insert_right (fake server send)",ret);

  xx_debug ("inferior task bsd server ports set up \nfs %x, ospn %x, oss %x\n",
	    fake_server,
	    original_server_port_name, original_server_send);

  /* A receive right to the reply generated by unix server exec() request */
  ret = mach_port_allocate(mach_task_self(), 
			   MACH_PORT_RIGHT_RECEIVE,
			   &exec_reply);
  CHK("create intercepted_reply_port port failed", ret);
    
  /* Pass this send right to Unix server so it replies to us after exec() */
  ret = mach_port_extract_right (mach_task_self (),
				 exec_reply,
				 MACH_MSG_TYPE_MAKE_SEND_ONCE,
				 &exec_reply_send,
				 &acquired);
  CHK("mach_port_extract_right (exec_reply)",ret);

  if (acquired != MACH_MSG_TYPE_PORT_SEND_ONCE)
    error("Incorrect right extracted, send once excpected for exec reply");

  ret = mach_port_move_member(mach_task_self(), 
			      fake_server,
			      inferior_wait_port_set);
  CHK ("Moving fake syscall port to inferior_wait_port_set", ret);

  xx_debug ("syscall fake server set up, resuming inferior\n");
  
  ret = task_resume (inferior_task);
  CHK("task_resume (startup)", ret);
	
  /* Read requests from the inferior.
     Pass directly through everything else except exec() calls.
   */
  while(exec_counter > 0)
    {
      ret = mach_msg (&syscall_in.header,	/* header */
		      MACH_RCV_MSG,		/* options */
		      0,			/* send size */
		      sizeof (struct syscall_msg_t), /* receive size */
		      inferior_wait_port_set,        /* receive_name */
		      MACH_MSG_TIMEOUT_NONE,
		      MACH_PORT_NULL);
      CHK("mach_msg (intercepted sycall)", ret);
	    
#ifdef DUMP_SYSCALL
      print_msg (&syscall_in.header);
#endif

      /* ASSERT : msgh_local_port == fake_server */

      if (notify_server (&syscall_in.header, &syscall_out.header))
	error ("received a notify while intercepting syscalls");

      if (syscall_in.header.msgh_id == MIG_EXEC_SYSCALL_ID)
	{
	  xx_debug ("Received EXEC SYSCALL, counter = %d\n", exec_counter);
	  if (exec_counter == 1)
	    {
	      original_exec_reply = syscall_in.header.msgh_remote_port;
	      syscall_in.header.msgh_remote_port = exec_reply_send;
	    }

	  if (!terminal_initted)
	    {
	      /* Now that the child has exec'd we know it has already set its
		 process group.  On POSIX systems, tcsetpgrp will fail with
		 EPERM if we try it before the child's setpgid.  */

	      /* Set up the "saved terminal modes" of the inferior
		 based on what modes we are starting it with.  */
	      target_terminal_init ();

	      /* Install inferior's terminal modes.  */
	      target_terminal_inferior ();

	      terminal_initted = 1;
	    }

	  exec_counter--;
	}
	    
      syscall_in.header.msgh_local_port  = syscall_in.header.msgh_remote_port;
      syscall_in.header.msgh_remote_port = original_server_send;

      reverse_msg_bits(&syscall_in.header, MACH_MSG_TYPE_COPY_SEND);

      ret = mach_msg_send (&syscall_in.header);
      CHK ("Forwarded syscall", ret);
    }
	
  ret = mach_port_move_member(mach_task_self(), 
			      fake_server,
			      MACH_PORT_NULL);
  CHK ("Moving fake syscall out of inferior_wait_port_set", ret);

  ret = mach_port_move_member(mach_task_self(), 
			      exec_reply,
			      inferior_wait_port_set);
  CHK ("Moving exec_reply to inferior_wait_port_set", ret);

  ret = mach_msg (&syscall_in.header,	/* header */
		  MACH_RCV_MSG,		/* options */
		  0,			/* send size */
		  sizeof (struct syscall_msg_t),	/* receive size */
		  inferior_wait_port_set,		/* receive_name */
		  MACH_MSG_TIMEOUT_NONE,
		  MACH_PORT_NULL);
  CHK("mach_msg (exec reply)", ret);

  ret = task_suspend (inferior_task);
  CHK ("Suspending inferior after last exec", ret);

  must_suspend_thread = 0;

  xx_debug ("Received exec reply from bsd server, suspended inferior task\n");

#ifdef DUMP_SYSCALL
      print_msg (&syscall_in.header);
#endif

  /* Message should appear as if it came from the unix server */
  syscall_in.header.msgh_local_port = MACH_PORT_NULL;

  /*  and go to the inferior task original reply port */
  syscall_in.header.msgh_remote_port = original_exec_reply;

  reverse_msg_bits(&syscall_in.header, MACH_MSG_TYPE_MOVE_SEND_ONCE);

  ret = mach_msg_send (&syscall_in.header);
  CHK ("Forwarding exec reply to inferior", ret);

  /* Garbage collect */
  ret = mach_port_deallocate (inferior_task,
			      original_server_port_name);
  CHK ("deallocating fake server send right", ret);

  ret = mach_port_insert_right (inferior_task,
				original_server_port_name,
				original_server_send,
				MACH_MSG_TYPE_MOVE_SEND);
  CHK ("Restoring the original bsd server send right", ret);

  ret = mach_port_destroy (mach_task_self (),
			   fake_server);
  fake_server = MACH_PORT_DEAD;
  CHK("mach_port_destroy (fake_server)", ret);

  ret = mach_port_destroy (mach_task_self (),
			   exec_reply);
  exec_reply = MACH_PORT_DEAD;
  CHK("mach_port_destroy (exec_reply)", ret);

  xx_debug ("Done with exec call interception\n");
}

void
consume_send_rights (thread_list, thread_count)
     thread_array_t thread_list;
     int            thread_count;
{
  int index;

  if (!thread_count)
    return;

  for (index = 0; index < thread_count; index++)
    {
      /* Since thread kill command kills threads, don't check ret */
      (void) mach_port_deallocate (mach_task_self (),
				   thread_list [ index ]);
    }
}

/* suspend/abort/resume a thread. */
setup_thread (thread, what)
     mach_port_t thread;
     int what;
{
  kern_return_t ret;

  if (what)
    {
      ret = thread_suspend (thread);
      CHK ("setup_thread thread_suspend", ret);
      
      ret = thread_abort (thread);
      CHK ("setup_thread thread_abort", ret);
    }
  else
    {
      ret = thread_resume (thread);
      CHK ("setup_thread thread_resume", ret);
    }
}

int
map_slot_to_mid (slot, threads, thread_count)
     int slot;
     thread_array_t threads;
     int thread_count;
{
  kern_return_t ret;
  int deallocate = 0;
  int index;
  int mid;

  if (! threads)
    {
      deallocate++;
      ret = task_threads (inferior_task, &threads, &thread_count);
      CHK ("Can not select a thread from a dead task", ret);
    }
  
  if (slot < 0 || slot >= thread_count)
    {
      if (deallocate)
	{
	  consume_send_rights (threads, thread_count);
	  (void) vm_deallocate (mach_task_self(), (vm_address_t)threads, 
				(thread_count * sizeof(mach_port_t)));
	}
      if (slot < 0)
	error ("invalid slot number");
      else
	return -(slot+1);
    }

  mid = map_port_name_to_mid (threads [slot], MACH_TYPE_THREAD);

  if (deallocate)
    {
      consume_send_rights (threads, thread_count);
      (void) vm_deallocate (mach_task_self(), (vm_address_t)threads, 
			    (thread_count * sizeof(mach_port_t)));
    }

  return mid;
}

static int
parse_thread_id (arg, thread_count, slots)
     char *arg;
     int thread_count;
     int slots;
{
  kern_return_t ret;
  int mid;
  int slot;
  int index;
  
  if (arg == 0)
    return 0;
  
  while (*arg && (*arg == ' ' || *arg == '\t'))
    arg++;
  
  if (! *arg)
    return 0;
  
  /* Currently parse MID and @SLOTNUMBER */
  if (*arg != '@')
    {
      mid = atoi (arg);
      if (mid <= 0)
	error ("valid thread mid expected");
      return mid;
    }
  
  arg++;
  slot = atoi (arg);

  if (slot < 0)
    error ("invalid slot number");

  /* If you want slot numbers to remain slot numbers, set slots.
   *
   * Well, since 0 is reserved, return the ordinal number
   * of the thread rather than the slot number. Awk, this
   * counts as a kludge.
   */
  if (slots)
    return -(slot+1);

  if (thread_count && slot >= thread_count)
    return -(slot+1);

  mid = map_slot_to_mid (slot);
  
  return mid;
}

/* THREAD_ID 0 is special; it selects the first kernel
 * thread from the list (i.e. SLOTNUMBER 0)
 * This is used when starting the program with 'run' or when attaching.
 *
 * If FLAG is 0 the context is not changed, and the registers, frame, etc
 * will continue to describe the old thread.
 *
 * If FLAG is nonzero, really select the thread.
 * If FLAG is 2, the THREAD_ID is a slotnumber instead of a mid.
 * 
 */
kern_return_t
select_thread (task, thread_id, flag)
     mach_port_t task;
     int thread_id;
     int flag;
{
  thread_array_t thread_list;
  int thread_count;
  kern_return_t ret;
  int index;
  thread_t new_thread = MACH_PORT_NULL;

  if (thread_id < 0)
    error ("Can't select cprocs without kernel thread");

  ret = task_threads (task, &thread_list, &thread_count);
  if (ret != KERN_SUCCESS)
    {
      warning ("Can not select a thread from a dead task");
      m3_kill_inferior ();
      return KERN_FAILURE;
    }

  if (thread_count == 0)
    {
      /* The task can not do anything anymore, but it still
       * exists as a container for memory and ports.
       */
      registers_changed ();
      warning ("Task %d has no threads",
	       map_port_name_to_mid (task, MACH_TYPE_TASK));
      current_thread = MACH_PORT_NULL;
      (void) vm_deallocate(mach_task_self(),
			   (vm_address_t) thread_list,
			   (thread_count * sizeof(mach_port_t)));
      return KERN_FAILURE;
    }

  if (! thread_id || flag == 2)
    {
      /* First thread or a slotnumber */
      if (! thread_id)
	new_thread = thread_list[0];
      else
	{
	  if (thread_id < thread_count)
	    new_thread = thread_list[ thread_id ];
	  else
	    {
	      (void) vm_deallocate(mach_task_self(),
				   (vm_address_t) thread_list,
				   (thread_count * sizeof(mach_port_t)));
	      error ("No such thread slot number : %d", thread_id);
	    }
	}
    }
  else
    {
      for (index = 0; index < thread_count; index++)
	if (thread_id == map_port_name_to_mid (thread_list [index],
					       MACH_TYPE_THREAD))
	  {
	    new_thread = thread_list [index];
	    index = -1;
	    break;
	  }
      
      if (index != -1)
	error ("No thread with mid %d", thread_id);
    }
  
  /* Notify when the selected thread dies */
  request_notify (new_thread, MACH_NOTIFY_DEAD_NAME, MACH_TYPE_THREAD);
  
  ret = vm_deallocate(mach_task_self(),
		      (vm_address_t) thread_list,
		      (thread_count * sizeof(mach_port_t)));
  CHK ("vm_deallocate", ret);
  
  if (! flag)
    current_thread = new_thread;
  else
    {
#if 0
      if (MACH_PORT_VALID (current_thread))
	{
	  /* Store the gdb's view of the thread we are deselecting
	   *
	   * @@ I think gdb updates registers immediately when they are
	   * changed, so don't do this.
	   */
	  ret = thread_abort (current_thread);
	  CHK ("Could not abort system calls when saving state of old thread",
	       ret);
	  target_prepare_to_store ();
	  target_store_registers (-1);
	}
#endif

      registers_changed ();

      current_thread = new_thread;

      ret = thread_abort (current_thread);
      CHK ("Could not abort system calls when selecting a thread", ret);

      stop_pc = read_pc();
      flush_cached_frames ();

      select_frame (get_current_frame (), 0);
    }

  return KERN_SUCCESS;
}

/*
 * Switch to use thread named NEW_THREAD.
 * Return it's MID
 */
int
switch_to_thread (new_thread)
     thread_t new_thread;
{
  thread_t saved_thread = current_thread;
  int mid;

  mid = map_port_name_to_mid (new_thread,
			      MACH_TYPE_THREAD);
  if (mid == -1)
    warning ("Can't map thread name 0x%x to mid", new_thread);
  else if (select_thread (inferior_task, mid, 1) != KERN_SUCCESS)
    {
      if (current_thread)
	current_thread = saved_thread;
      error ("Could not select thread %d", mid);
    }
	
  return mid;
}

/* Do this in gdb after doing FORK but before STARTUP_INFERIOR.
 * Note that the registers are not yet valid in the inferior task.
 */
static void
m3_trace_him (pid)
     int pid;
{
  kern_return_t ret;

  push_target (&m3_ops);

  inferior_task = task_by_pid (pid);

  if (! MACH_PORT_VALID (inferior_task))
    error ("Can not map Unix pid %d to Mach task", pid);

  /* Clean up previous notifications and create new ones */
  setup_notify_port (1);

  /* When notification appears, the inferior task has died */
  request_notify (inferior_task, MACH_NOTIFY_DEAD_NAME, MACH_TYPE_TASK);

  emulator_present = have_emulator_p (inferior_task);

  /* By default, select the first thread,
   * If task has no threads, gives a warning
   * Does not fetch registers, since they are not yet valid.
   */
  select_thread (inferior_task, 0, 0);

  inferior_exception_port = MACH_PORT_NULL;

  setup_exception_port ();

  xx_debug ("Now the debugged task is created\n");

  /* One trap to exec the shell, one to exec the program being debugged.  */
  intercept_exec_calls (2);
}

setup_exception_port ()
{
  kern_return_t ret;

  ret = mach_port_allocate (mach_task_self(), 
			    MACH_PORT_RIGHT_RECEIVE,
			    &inferior_exception_port);
  CHK("mach_port_allocate",ret);

  /* add send right */
  ret = mach_port_insert_right (mach_task_self (),
				inferior_exception_port,
				inferior_exception_port,
				MACH_MSG_TYPE_MAKE_SEND);
  CHK("mach_port_insert_right",ret);

  ret = mach_port_move_member (mach_task_self(), 
			       inferior_exception_port,
			       inferior_wait_port_set);
  CHK("mach_port_move_member",ret);

  ret = task_get_special_port (inferior_task, 
			       TASK_EXCEPTION_PORT,
			       &inferior_old_exception_port);
  CHK ("task_get_special_port(old exc)",ret);

  ret = task_set_special_port (inferior_task,
			       TASK_EXCEPTION_PORT, 
			       inferior_exception_port);
  CHK("task_set_special_port",ret);

  ret = mach_port_deallocate (mach_task_self (),
			      inferior_exception_port);
  CHK("mack_port_deallocate",ret);

#if 0
  /* When notify appears, the inferior_task's exception
   * port has been destroyed.
   *
   * Not used, since the dead_name_notification already
   * appears when task dies.
   *
   */
  request_notify (inferior_exception_port,
		  MACH_NOTIFY_NO_SENDERS,
		  MACH_TYPE_EXCEPTION_PORT);
#endif
}

/* Nonzero if gdb is waiting for a message */
int mach_really_waiting;

/* Wait for the inferior to stop for some reason.
   - Loop on notifications until inferior_task dies.
   - Loop on exceptions until stopped_in_exception comes true.
     (e.g. we receive a single step trace trap)
   - a message arrives to gdb's message port

   There is no other way to exit this loop.

   Returns the inferior_pid for rest of gdb.
   Side effects: Set *OURSTATUS.  */
int
mach_really_wait (pid, ourstatus)
     int pid;
     struct target_waitstatus *ourstatus;
{
  kern_return_t ret;
  int w;

  struct msg {
    mach_msg_header_t    header;
    mach_msg_type_t foo;
    int             data[8000];
  } in_msg, out_msg;

  /* Either notify (death), exception or message can stop the inferior */
  stopped_in_exception = FALSE;

  while (1)
    {
      QUIT;

      stop_exception = stop_code = stop_subcode = -1;
      stop_thread = MACH_PORT_NULL;

      mach_really_waiting = 1;
      ret = mach_msg (&in_msg.header,		/* header */
		      MACH_RCV_MSG,		/* options */
		      0,			/* send size */
		      sizeof (struct msg),	/* receive size */
		      currently_waiting_for,	/* receive name */
		      MACH_MSG_TIMEOUT_NONE,
		      MACH_PORT_NULL);
      mach_really_waiting = 0;
      CHK("mach_msg (receive)", ret);

      /* Check if we received a notify of the childs' death */
      if (notify_server (&in_msg.header, &out_msg.header))
	{
	  /* If inferior_task is null then the inferior has
	     gone away and we want to return to command level.
	     Otherwise it was just an informative message and we
	     need to look to see if there are any more. */
	  if (inferior_task != MACH_PORT_NULL)
	    continue;
	  else
	    {
	      /* Collect Unix exit status for gdb */

	      wait3(&w, WNOHANG, 0);

	      /* This mess is here to check that the rest of
	       * gdb knows that the inferior died. It also
	       * tries to hack around the fact that Mach 3.0 (mk69)
	       * unix server (ux28) does not always know what
	       * has happened to it's children when mach-magic
	       * is applied on them.
	       */
	      if ((!WIFEXITED(w) && WIFSTOPPED(w))         ||
		  (WIFEXITED(w)  && WEXITSTATUS(w) > 0377))
		{
		  WSETEXIT(w, 0);
		  warning ("Using exit value 0 for terminated task");
		}
	      else if (!WIFEXITED(w))
		{
		  int sig = WTERMSIG(w);

		  /* Signals cause problems. Warn the user. */
		  if (sig != SIGKILL) /* Bad luck if garbage matches this */
		    warning ("The terminating signal stuff may be nonsense");
		  else if (sig > NSIG)
		    {
		      WSETEXIT(w, 0);
		      warning ("Using exit value 0 for terminated task");
		    }
		}
	      store_waitstatus (ourstatus, w);
	      return inferior_pid;
	    }
	}

      /* Hmm. Check for exception, as it was not a notification.
	 exc_server() does an upcall to catch_exception_raise()
	 if this rpc is an exception. Further actions are decided
	 there.
       */
      if (! exc_server (&in_msg.header, &out_msg.header))
	{

	  /* Not an exception, check for message.
	   *
	   * Messages don't come from the inferior, or if they
	   * do they better be asynchronous or it will hang.
	   */
	  if (gdb_message_server (&in_msg.header))
	    continue;

	  error ("Unrecognized message received in mach_really_wait");
	}

      /* Send the reply of the exception rpc to the suspended task */
      ret = mach_msg_send (&out_msg.header);
      CHK ("mach_msg_send (exc reply)", ret);
      
      if (stopped_in_exception)
	{
	  /* Get unix state. May be changed in mach3_exception_actions() */
	  wait3(&w, WNOHANG, 0);

	  mach3_exception_actions (&w, FALSE, "Task");

	  store_waitstatus (ourstatus, w);
	  return inferior_pid;
	}
    }
}

/* Called by macro DO_QUIT() in utils.c(quit).
 * This is called just before calling error() to return to command level
 */
void
mach3_quit ()
{
  int mid;
  kern_return_t ret;
  
  if (mach_really_waiting)
    {
      ret = task_suspend (inferior_task);
      
      if (ret != KERN_SUCCESS)
	{
	  warning ("Could not suspend task for interrupt: %s",
		   mach_error_string (ret));
	  mach_really_waiting = 0;
	  return;
	}
    }

  must_suspend_thread = 0;
  mach_really_waiting = 0;

  mid = map_port_name_to_mid (current_thread, MACH_TYPE_THREAD);
  if (mid == -1)
    {
      warning ("Selecting first existing kernel thread");
      mid = 0;
    }

  current_thread = MACH_PORT_NULL; /* Force setup */
  select_thread (inferior_task, mid, 1);

  return;
}

#if 0
/* bogus bogus bogus.  It is NOT OK to quit out of target_wait.  */

/* If ^C is typed when we are waiting for a message
 * and your Unix server is able to notice that we 
 * should quit now.
 *
 * Called by REQUEST_QUIT() from utils.c(request_quit)
 */
void
mach3_request_quit ()
{
  if (mach_really_waiting)
    immediate_quit = 1;
}      
#endif

/*
 * Gdb message server.
 * Currently implemented is the STOP message, that causes
 * gdb to return to the command level like ^C had been typed from terminal.
 */
int
gdb_message_server (InP)
     mach_msg_header_t *InP;
{
  kern_return_t ret;
  int mid;

  if (InP->msgh_local_port == our_message_port)
    {
      /* A message coming to our_message_port. Check validity */
      switch (InP->msgh_id) {

      case GDB_MESSAGE_ID_STOP:
	ret = task_suspend (inferior_task);
	if (ret != KERN_SUCCESS)
	  warning ("Could not suspend task for stop message: %s",
		   mach_error_string (ret));

	/* QUIT in mach_really_wait() loop. */
	request_quit (0);
	break;

      default:
	warning ("Invalid message id %d received, ignored.",
		 InP->msgh_id);
	break;
      }

      return 1;
    }

  /* Message not handled by this server */
  return 0;
}

/* NOTE: This is not an RPC call. It is a simpleroutine.
 *
 * This is not called from this gdb code.
 *
 * It may be called by another debugger to cause this
 * debugger to enter command level:
 *
 *            (gdb) set stop_inferior_gdb ()
 *            (gdb) continue
 *
 * External program "stop-gdb" implements this also.
 */
void
stop_inferior_gdb ()
{
  kern_return_t ret;

  /* Code generated by mig, with minor cleanups :-)
   *
   * simpleroutine stop_inferior_gdb (our_message_port : mach_port_t);
   */

  typedef struct {
    mach_msg_header_t Head;
  } Request;

  Request Mess;

  register Request *InP = &Mess;

  InP->Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);

  /* msgh_size passed as argument */
  InP->Head.msgh_remote_port = our_message_port;
  InP->Head.msgh_local_port  = MACH_PORT_NULL;
  InP->Head.msgh_seqno       = 0;
  InP->Head.msgh_id          = GDB_MESSAGE_ID_STOP;

  ret = mach_msg (&InP->Head,
		  MACH_SEND_MSG|MACH_MSG_OPTION_NONE,
		  sizeof(Request),
		  0,
		  MACH_PORT_NULL,
		  MACH_MSG_TIMEOUT_NONE,
		  MACH_PORT_NULL);
}

#ifdef THREAD_ALLOWED_TO_BREAK
/*
 * Return 1 if the MID specifies the thread that caused the
 * last exception.
 *  Since catch_exception_raise() selects the thread causing
 * the last exception to current_thread, we just check that
 * it is selected and the last exception was a breakpoint.
 */
int
mach_thread_for_breakpoint (mid)
     int mid;
{
  int cmid = map_port_name_to_mid (current_thread, MACH_TYPE_THREAD);

  if (mid < 0)
    {
      mid = map_slot_to_mid (-(mid+1), 0, 0);
      if (mid < 0)
	return 0;		/* Don't stop, no such slot */
    }

  if (! mid || cmid == -1)
    return 1;	/* stop */

  return cmid == mid && stop_exception == EXC_BREAKPOINT;
}
#endif /* THREAD_ALLOWED_TO_BREAK */

#ifdef THREAD_PARSE_ID
/*
 * Map a thread id string (MID or a @SLOTNUMBER)
 * to a thread-id.
 *
 *   0  matches all threads.
 *   Otherwise the meaning is defined only in this file.
 *   (mach_thread_for_breakpoint uses it)
 *
 * @@ This allows non-existent MIDs to be specified.
 *    It now also allows non-existent slots to be
 *    specified. (Slot numbers stored are negative,
 *    and the magnitude is one greater than the actual
 *    slot index. (Since 0 is reserved))
 */
int
mach_thread_parse_id (arg)
     char *arg;
{
  int mid;
  if (arg == 0)
    error ("thread id excpected");
  mid = parse_thread_id (arg, 0, 1);

  return mid;
}
#endif /* THREAD_PARSE_ID */

#ifdef THREAD_OUTPUT_ID
char *
mach_thread_output_id (mid)
     int mid;
{
  static char foobar [20];

  if (mid > 0)
    sprintf (foobar, "mid %d", mid);
  else if (mid < 0)
    sprintf (foobar, "@%d", -(mid+1));
  else
    sprintf (foobar, "*any thread*");

  return foobar;
}
#endif /* THREAD_OUTPUT_ID */

/* Called with hook PREPARE_TO_PROCEED() from infrun.c.
 *
 * If we have switched threads and stopped at breakpoint return 1 otherwise 0.
 *
 *  if SELECT_IT is nonzero, reselect the thread that was active when
 *  we stopped at a breakpoint.
 *
 */

mach3_prepare_to_proceed (select_it)
     int select_it;
{
  if (stop_thread &&
      stop_thread != current_thread &&
      stop_exception == EXC_BREAKPOINT)
    {
      int mid;

      if (! select_it)
	return 1;

      mid = switch_to_thread (stop_thread);

      return 1;
    }

  return 0;
}

/* this stuff here is an upcall via libmach/excServer.c 
   and mach_really_wait which does the actual upcall.

   The code will pass the exception to the inferior if:

     - The task that signaled is not the inferior task
       (e.g. when debugging another debugger)

     - The user has explicitely requested to pass on the exceptions.
       (e.g to the default unix exception handler, which maps
	exceptions to signals, or the user has her own exception handler)

     - If the thread that signaled is being single-stepped and it
       has set it's own exception port and the exception is not
       EXC_BREAKPOINT. (Maybe this is not desirable?)
 */

kern_return_t
catch_exception_raise (port, thread, task, exception, code, subcode)
     mach_port_t port;
     thread_t thread;
     task_t task;
     int exception, code, subcode;
{
  kern_return_t ret;
  boolean_t signal_thread;
  int mid = map_port_name_to_mid (thread, MACH_TYPE_THREAD);

  if (! MACH_PORT_VALID (thread))
    {
      /* If the exception was sent and thread dies before we
	 receive it, THREAD will be MACH_PORT_DEAD
       */

      current_thread = thread = MACH_PORT_NULL;
      error ("Received exception from nonexistent thread");
    }

  /* Check if the task died in transit.
   * @@ Isn't the thread also invalid in such case?
   */
  if (! MACH_PORT_VALID (task))
    {
      current_thread = thread = MACH_PORT_NULL;
      error ("Received exception from nonexistent task");
    }

  if (exception < 0 || exception > MAX_EXCEPTION)
    fatal ("catch_exception_raise: unknown exception code %d thread %d",
	   exception,
	   mid);

  if (! MACH_PORT_VALID (inferior_task))
    error ("got an exception, but inferior_task is null or dead");
  
  stop_exception = exception;
  stop_code      = code;
  stop_subcode   = subcode;  
  stop_thread    = thread;
  
  signal_thread = exception != EXC_BREAKPOINT       &&
    		  port == singlestepped_thread_port &&
		  MACH_PORT_VALID (thread_saved_exception_port);

  /* If it was not our inferior or if we want to forward
   * the exception to the inferior's handler, do it here
   *
   * Note: If you have forwarded EXC_BREAKPOINT I trust you know why.
   */
  if (task != inferior_task ||
      signal_thread         ||
      exception_map [exception].forward)
    {
      mach_port_t eport = inferior_old_exception_port;

      if (signal_thread)
	{
	  /*
	    GDB now forwards the exeption to thread's original handler,
	    since the user propably knows what he is doing.
	    Give a message, though.
	   */

	  mach3_exception_actions ((WAITTYPE *)NULL, TRUE, "Thread");
	  eport = thread_saved_exception_port;
	}

      /* Send the exception to the original handler */
      ret = exception_raise (eport,
			     thread, 
			     task,
			     exception,
			     code,
			     subcode);

      (void) mach_port_deallocate (mach_task_self (), task);
      (void) mach_port_deallocate (mach_task_self (), thread);

      /* If we come here, we don't want to trace any more, since we
       * will never stop for tracing anyway.
       */
      discard_single_step (thread);

      /* Do not stop the inferior */
      return ret;
    }
  
  /* Now gdb handles the exception */
  stopped_in_exception = TRUE;

  ret = task_suspend (task);
  CHK ("Error suspending inferior after exception", ret);

  must_suspend_thread = 0;

  if (current_thread != thread)
    {
      if (MACH_PORT_VALID (singlestepped_thread_port))
	/* Cleanup discards single stepping */
	error ("Exception from thread %d while singlestepping thread %d",
	       mid,
	       map_port_name_to_mid (current_thread, MACH_TYPE_THREAD));
      
      /* Then select the thread that caused the exception */
      if (select_thread (inferior_task, mid, 0) != KERN_SUCCESS)
	error ("Could not select thread %d causing exception", mid);
      else
	warning ("Gdb selected thread %d", mid);
    }

  /* If we receive an exception that is not breakpoint
   * exception, we interrupt the single step and return to
   * debugger. Trace condition is cleared.
   */
  if (MACH_PORT_VALID (singlestepped_thread_port))
    {
      if (stop_exception != EXC_BREAKPOINT)
	warning ("Single step interrupted by exception");
      else if (port == singlestepped_thread_port)
	{
	  /* Single step exception occurred, remove trace bit
	   * and return to gdb.
	   */
	  if (! MACH_PORT_VALID (current_thread))
	    error ("Single stepped thread is not valid");
	
	  /* Resume threads, but leave the task suspended */
	  resume_all_threads (0);
	}
      else
	warning ("Breakpoint while single stepping?");

      discard_single_step (current_thread);
    }
  
  (void) mach_port_deallocate (mach_task_self (), task);
  (void) mach_port_deallocate (mach_task_self (), thread);

  return KERN_SUCCESS;
}

int
port_valid (port, mask)
  mach_port_t port;
  int         mask;
{
  kern_return_t ret;
  mach_port_type_t type;

  ret = mach_port_type (mach_task_self (),
			port,
			&type);
  if (ret != KERN_SUCCESS || (type & mask) != mask)
    return 0;
  return 1;
}

/* @@ No vm read cache implemented yet */
boolean_t vm_read_cache_valid = FALSE;

/*
 * Read inferior task's LEN bytes from ADDR and copy it to MYADDR
 * in gdb's address space.
 *
 * Return 0 on failure; number of bytes read otherwise.
 */
int
mach3_read_inferior (addr, myaddr, length)
     CORE_ADDR addr;
     char *myaddr;
     int length;
{
  kern_return_t ret;
  vm_address_t low_address       = (vm_address_t) trunc_page (addr);
  vm_size_t    aligned_length = 
    			(vm_size_t) round_page (addr+length) - low_address;
  pointer_t    copied_memory;
  int	       copy_count;

  /* Get memory from inferior with page aligned addresses */
  ret = vm_read (inferior_task,
		 low_address,
		 aligned_length,
		 &copied_memory,
		 &copy_count);
  if (ret != KERN_SUCCESS)
    {
      /* the problem is that the inferior might be killed for whatever reason
       * before we go to mach_really_wait. This is one place that ought to
       * catch many of those errors.
       * @@ A better fix would be to make all external events to GDB
       * to arrive via a SINGLE port set. (Including user input!)
       */

      if (! port_valid (inferior_task, MACH_PORT_TYPE_SEND))
	{
	  m3_kill_inferior ();
	  error ("Inferior killed (task port invalid)");
	}
      else
	{
#ifdef OSF
	  extern int errno;
	  /* valprint.c gives nicer format if this does not
	     screw it. Eamonn seems to like this, so I enable
	     it if OSF is defined...
	   */
	  warning ("[read inferior %x failed: %s]",
		   addr, mach_error_string (ret));
	  errno = 0;
#endif
	  return 0;
	}
    }

  memcpy (myaddr, (char *)addr - low_address + copied_memory, length);

  ret = vm_deallocate (mach_task_self (),
		       copied_memory,
		       copy_count);
  CHK("mach3_read_inferior vm_deallocate failed", ret);

  return length;
}

#ifdef __STDC__
#define CHK_GOTO_OUT(str,ret) \
  do if (ret != KERN_SUCCESS) { errstr = #str; goto out; } while(0)
#else
#define CHK_GOTO_OUT(str,ret) \
  do if (ret != KERN_SUCCESS) { errstr = str; goto out; } while(0)
#endif

struct vm_region_list {
  struct vm_region_list *next;
  vm_prot_t	protection;
  vm_address_t  start;
  vm_size_t	length;
};

struct obstack  region_obstack;

/*
 * Write inferior task's LEN bytes from ADDR and copy it to MYADDR
 * in gdb's address space.
 */
int
mach3_write_inferior (addr, myaddr, length)
     CORE_ADDR addr;
     char *myaddr;
     int length;
{
  kern_return_t ret;
  vm_address_t low_address       = (vm_address_t) trunc_page (addr);
  vm_size_t    aligned_length = 
    			(vm_size_t) round_page (addr+length) - low_address;
  pointer_t    copied_memory;
  int	       copy_count;
  int	       deallocate = 0;

  char         *errstr = "Bug in mach3_write_inferior";

  struct vm_region_list *region_element;
  struct vm_region_list *region_head = (struct vm_region_list *)NULL;

  /* Get memory from inferior with page aligned addresses */
  ret = vm_read (inferior_task,
		 low_address,
		 aligned_length,
		 &copied_memory,
		 &copy_count);
  CHK_GOTO_OUT ("mach3_write_inferior vm_read failed", ret);

  deallocate++;

  memcpy ((char *)addr - low_address + copied_memory, myaddr, length);

  obstack_init (&region_obstack);

  /* Do writes atomically.
   * First check for holes and unwritable memory.
   */
  {
    vm_size_t    remaining_length  = aligned_length;
    vm_address_t region_address    = low_address;

    struct vm_region_list *scan;

    while(region_address < low_address + aligned_length)
      {
	vm_prot_t protection;
	vm_prot_t max_protection;
	vm_inherit_t inheritance;
	boolean_t shared;
	mach_port_t object_name;
	vm_offset_t offset;
	vm_size_t   region_length = remaining_length;
	vm_address_t old_address  = region_address;
    
	ret = vm_region (inferior_task,
			 &region_address,
			 &region_length,
			 &protection,
			 &max_protection,
			 &inheritance,
			 &shared,
			 &object_name,
			 &offset);
	CHK_GOTO_OUT ("vm_region failed", ret);

	/* Check for holes in memory */
	if (old_address != region_address)
	  {
	    warning ("No memory at 0x%x. Nothing written",
		     old_address);
	    ret = KERN_SUCCESS;
	    length = 0;
	    goto out;
	  }

	if (!(max_protection & VM_PROT_WRITE))
	  {
	    warning ("Memory at address 0x%x is unwritable. Nothing written",
		     old_address);
	    ret = KERN_SUCCESS;
	    length = 0;
	    goto out;
	  }

	/* Chain the regions for later use */
	region_element = 
	  (struct vm_region_list *)
	    obstack_alloc (&region_obstack, sizeof (struct vm_region_list));
    
	region_element->protection = protection;
	region_element->start      = region_address;
	region_element->length     = region_length;

	/* Chain the regions along with protections */
	region_element->next = region_head;
	region_head          = region_element;
	
	region_address += region_length;
	remaining_length = remaining_length - region_length;
      }

    /* If things fail after this, we give up.
     * Somebody is messing up inferior_task's mappings.
     */
    
    /* Enable writes to the chained vm regions */
    for (scan = region_head; scan; scan = scan->next)
      {
	boolean_t protection_changed = FALSE;
	
	if (!(scan->protection & VM_PROT_WRITE))
	  {
	    ret = vm_protect (inferior_task,
			      scan->start,
			      scan->length,
			      FALSE,
			      scan->protection | VM_PROT_WRITE);
	    CHK_GOTO_OUT ("vm_protect: enable write failed", ret);
	  }
      }

    ret = vm_write (inferior_task,
		    low_address,
		    copied_memory,
		    aligned_length);
    CHK_GOTO_OUT ("vm_write failed", ret);
	
    /* Set up the original region protections, if they were changed */
    for (scan = region_head; scan; scan = scan->next)
      {
	boolean_t protection_changed = FALSE;
	
	if (!(scan->protection & VM_PROT_WRITE))
	  {
	    ret = vm_protect (inferior_task,
			      scan->start,
			      scan->length,
			      FALSE,
			      scan->protection);
	    CHK_GOTO_OUT ("vm_protect: enable write failed", ret);
	  }
      }
  }

 out:
  if (deallocate)
    {
      obstack_free (&region_obstack, 0);
      
      (void) vm_deallocate (mach_task_self (),
			    copied_memory,
			    copy_count);
    }

  if (ret != KERN_SUCCESS)
    {
      warning ("%s %s", errstr, mach_error_string (ret));
      return 0;
    }

  return length;
}

/* Return 0 on failure, number of bytes handled otherwise.  */
static int
m3_xfer_memory (memaddr, myaddr, len, write, target)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
     struct target_ops *target;	/* IGNORED */
{
  int result;

  if (write)
    result = mach3_write_inferior (memaddr, myaddr, len);
  else
    result = mach3_read_inferior  (memaddr, myaddr, len);

  return result;
}


static char *
translate_state(state)
int	state;
{
  switch (state) {
  case TH_STATE_RUNNING:	return("R");
  case TH_STATE_STOPPED:	return("S");
  case TH_STATE_WAITING:	return("W");
  case TH_STATE_UNINTERRUPTIBLE: return("U");
  case TH_STATE_HALTED:		return("H");
  default:			return("?");
  }
}

static char *
translate_cstate (state)
     int state;
{
  switch (state)
    {
    case CPROC_RUNNING:	return "R";
    case CPROC_SWITCHING: return "S";
    case CPROC_BLOCKED: return "B";
    case CPROC_CONDWAIT: return "C";
    case CPROC_CONDWAIT|CPROC_SWITCHING: return "CS";
    default: return "?";
    }
}

/* type == MACH_MSG_TYPE_COPY_SEND || type == MACH_MSG_TYPE_MAKE_SEND */

mach_port_t           /* no mach_port_name_t found in include files. */
map_inferior_port_name (inferior_name, type)
     mach_port_t inferior_name;
     mach_msg_type_name_t type;
{
  kern_return_t        ret;
  mach_msg_type_name_t acquired;
  mach_port_t          iport;
  
  ret = mach_port_extract_right (inferior_task,
				 inferior_name,
				 type,
				 &iport,
				 &acquired);
  CHK("mach_port_extract_right (map_inferior_port_name)", ret);

  if (acquired != MACH_MSG_TYPE_PORT_SEND)
    error("Incorrect right extracted, (map_inferior_port_name)");

  ret = mach_port_deallocate (mach_task_self (),
			      iport);
  CHK ("Deallocating mapped port (map_inferior_port_name)", ret);

  return iport;
}

/*
 * Naming convention:
 *  Always return user defined name if found.
 *  _K == A kernel thread with no matching CPROC
 *  _C == A cproc with no current cthread
 *  _t == A cthread with no user defined name
 *
 * The digits that follow the _names are the SLOT number of the
 * kernel thread if there is such a thing, otherwise just a negation
 * of the sequential number of such cprocs.
 */

static char buf[7];

static char *
get_thread_name (one_cproc, id)
     gdb_thread_t one_cproc;
     int id;
{
  if (one_cproc)
    if (one_cproc->cthread == NULL)
      {
	/* cproc not mapped to any cthread */
	sprintf(buf, "_C%d", id);
      }
    else if (! one_cproc->cthread->name)
      {
	/* cproc and cthread, but no name */
	sprintf(buf, "_t%d", id);
      }
    else
      return (char *)(one_cproc->cthread->name);
  else
    {
      if (id < 0)
	warning ("Inconsistency in thread name id %d", id);

      /* Kernel thread without cproc */
      sprintf(buf, "_K%d", id);
    }

  return buf;
}

int
fetch_thread_info (task, mthreads_out)
     mach_port_t	task;
     gdb_thread_t	*mthreads_out;	/* out */
{
  kern_return_t  ret;
  thread_array_t th_table;
  int		 th_count;
  gdb_thread_t mthreads = NULL;
  int 		 index;

  ret = task_threads (task, &th_table, &th_count);
  if (ret != KERN_SUCCESS)
    {
      warning ("Error getting inferior's thread list:%s",
	       mach_error_string(ret));
      m3_kill_inferior ();
      return -1;
    }
  
  mthreads = (gdb_thread_t)
    		obstack_alloc
		  (cproc_obstack,
		   th_count * sizeof (struct gdb_thread));

  for (index = 0; index < th_count; index++)
    {
      thread_t saved_thread = MACH_PORT_NULL;
      int mid;

      if (must_suspend_thread)
	setup_thread (th_table[ index ], 1);

      if (th_table[index] != current_thread)
	{
	  saved_thread = current_thread;
	  
	  mid = switch_to_thread (th_table[ index ]);
	}

      mthreads[index].name  = th_table[index];
      mthreads[index].cproc = NULL;	/* map_cprocs_to_kernel_threads() */
      mthreads[index].in_emulator = FALSE;
      mthreads[index].slotid = index;
      
      mthreads[index].sp = read_register (SP_REGNUM);
      mthreads[index].fp = read_register (FP_REGNUM);
      mthreads[index].pc = read_pc ();

      if (MACH_PORT_VALID (saved_thread))
	mid = switch_to_thread (saved_thread);

      if (must_suspend_thread)
	setup_thread (th_table[ index ], 0);
    }
  
  consume_send_rights (th_table, th_count);
  ret = vm_deallocate (mach_task_self(), (vm_address_t)th_table, 
		       (th_count * sizeof(mach_port_t)));
  if (ret != KERN_SUCCESS)
    {
      warning ("Error trying to deallocate thread list : %s",
	       mach_error_string (ret));
    }

  *mthreads_out = mthreads;

  return th_count;
}


/*
 * Current emulator always saves the USP on top of
 * emulator stack below struct emul_stack_top stuff.
 */
CORE_ADDR
fetch_usp_from_emulator_stack (sp)
     CORE_ADDR sp;
{
  CORE_ADDR stack_pointer;

  sp = (sp & ~(EMULATOR_STACK_SIZE-1)) +
    	EMULATOR_STACK_SIZE - sizeof (struct emul_stack_top);
  
  if (mach3_read_inferior (sp,
			   &stack_pointer,
			   sizeof (CORE_ADDR)) != sizeof (CORE_ADDR))
    {
      warning ("Can't read user sp from emulator stack address 0x%x", sp);
      return 0;
    }

  return stack_pointer;
}

#ifdef MK67

/* get_emulation_vector() interface was changed after mk67 */
#define EMUL_VECTOR_COUNT 400	/* Value does not matter too much */

#endif /* MK67 */

/* Check if the emulator exists at task's address space.
 */
boolean_t
have_emulator_p (task)
     task_t task;
{
  kern_return_t	ret;
#ifndef EMUL_VECTOR_COUNT
  vm_offset_t	*emulation_vector;
  int		n;
#else
  vm_offset_t	emulation_vector[ EMUL_VECTOR_COUNT ];
  int		n = EMUL_VECTOR_COUNT;
#endif
  int		i;
  int		vector_start;
  
  ret = task_get_emulation_vector (task,
				   &vector_start,
#ifndef EMUL_VECTOR_COUNT
				   &emulation_vector,
#else
				   emulation_vector,
#endif
				   &n);
  CHK("task_get_emulation_vector", ret);
  xx_debug ("%d vectors from %d at 0x%08x\n",
	    n, vector_start, emulation_vector);
  
  for(i = 0; i < n; i++)
    {
      vm_offset_t entry = emulation_vector [i];

      if (EMULATOR_BASE <= entry && entry <= EMULATOR_END)
	return TRUE;
      else if (entry)
	{
	  static boolean_t informed = FALSE;
	  if (!informed)
	    {
	      warning("Emulation vector address 0x08%x outside emulator space",
		      entry);
	      informed = TRUE;
	    }
	}
    }
  return FALSE;
}

/* Map cprocs to kernel threads and vice versa.  */

void
map_cprocs_to_kernel_threads (cprocs, mthreads, thread_count)
     gdb_thread_t cprocs;
     gdb_thread_t mthreads;
     int thread_count;
{
  int index;
  gdb_thread_t scan;
  boolean_t all_mapped = TRUE;
  LONGEST stack_base;
  LONGEST stack_size;

  for (scan = cprocs; scan; scan = scan->next)
    {
      /* Default to: no kernel thread for this cproc */
      scan->reverse_map = -1;

      /* Check if the cproc is found by its stack */
      for (index = 0; index < thread_count; index++)
	{
	  stack_base =
	    extract_signed_integer (scan->raw_cproc + CPROC_BASE_OFFSET,
				    CPROC_BASE_SIZE);
	  stack_size = 
	    extract_signed_integer (scan->raw_cproc + CPROC_SIZE_OFFSET,
				    CPROC_SIZE_SIZE);
	  if ((mthreads + index)->sp > stack_base &&
	      (mthreads + index)->sp <= stack_base + stack_size)
	    {
	      (mthreads + index)->cproc = scan;
	      scan->reverse_map = index;
	      break;
	    }
	}
      all_mapped &= (scan->reverse_map != -1);
    }

  /* Check for threads that are currently in the emulator.
   * If so, they have a different stack, and the still unmapped
   * cprocs may well get mapped to these threads.
   * 
   * If:
   *  - cproc stack does not match any kernel thread stack pointer
   *  - there is at least one extra kernel thread
   *    that has no cproc mapped above.
   *  - some kernel thread stack pointer points to emulator space
   *  then we find the user stack pointer saved in the emulator
   *  stack, and try to map that to the cprocs.
   *
   * Also set in_emulator for kernel threads.
   */ 

  if (emulator_present)
    {
      for (index = 0; index < thread_count; index++)
	{
	  CORE_ADDR emul_sp;
	  CORE_ADDR usp;

	  gdb_thread_t mthread = (mthreads+index);
	  emul_sp = mthread->sp;

	  if (mthread->cproc == NULL &&
	      EMULATOR_BASE <= emul_sp && emul_sp <= EMULATOR_END)
	    {
	      mthread->in_emulator = emulator_present;
	      
	      if (!all_mapped && cprocs)
		{
		  usp = fetch_usp_from_emulator_stack (emul_sp);
		  
		  /* @@ Could be more accurate */
		  if (! usp)
		    error ("Zero stack pointer read from emulator?");
		  
		  /* Try to match this stack pointer to the cprocs that
		   * don't yet have a kernel thread.
		   */
		  for (scan = cprocs; scan; scan = scan->next)
		    {
		      
		      /* Check is this unmapped CPROC stack contains
		       * the user stack pointer saved in the
		       * emulator.
		       */
		      if (scan->reverse_map == -1)
			{
			  stack_base =
			    extract_signed_integer
			      (scan->raw_cproc + CPROC_BASE_OFFSET,
			       CPROC_BASE_SIZE);
			  stack_size = 
			    extract_signed_integer
			      (scan->raw_cproc + CPROC_SIZE_OFFSET,
			       CPROC_SIZE_SIZE);
			  if (usp > stack_base &&
			      usp <= stack_base + stack_size)
			    {
			      mthread->cproc = scan;
			      scan->reverse_map = index;
			      break;
			    }
			}
		    }
		}
	    }
	}
    }
}

/*
 * Format of the thread_list command
 *
 * 	             slot mid sel   name  emul ks susp  cstate wired   address
 */
#define TL_FORMAT "%-2.2s %5d%c %-10.10s %1.1s%s%-5.5s %-2.2s %-5.5s "

#define TL_HEADER "\n@    MID  Name        KState CState   Where\n"

void
print_tl_address (stream, pc)
     GDB_FILE *stream;
     CORE_ADDR pc;
{
  if (! lookup_minimal_symbol_by_pc (pc))
    fprintf_filtered (stream, local_hex_format(), pc);
  else
    {
      extern int addressprint;
      extern int asm_demangle;

      int store    = addressprint;
      addressprint = 0;
      print_address_symbolic (pc, stream, asm_demangle, "");
      addressprint = store;
    }
}

/* For thread names, but also for gdb_message_port external name */
#define MAX_NAME_LEN 50

/* Returns the address of variable NAME or 0 if not found */
CORE_ADDR
lookup_address_of_variable (name)
     char *name;
{
  struct symbol *sym;
  CORE_ADDR symaddr = 0;
  struct minimal_symbol *msymbol;

  sym = lookup_symbol (name,
		       (struct block *)NULL,
		       VAR_NAMESPACE,
		       (int *)NULL,
		       (struct symtab **)NULL);

  if (sym)
    symaddr = SYMBOL_VALUE (sym);

  if (! symaddr)
    {
      msymbol = lookup_minimal_symbol (name, NULL, NULL);

      if (msymbol && msymbol->type == mst_data)
	symaddr = SYMBOL_VALUE_ADDRESS (msymbol);
    }

  return symaddr;
}

static gdb_thread_t
get_cprocs()
{
  gdb_thread_t cproc_head;
  gdb_thread_t cproc_copy;
  CORE_ADDR their_cprocs;
  char *buf[TARGET_PTR_BIT / HOST_CHAR_BIT];
  char *name;
  cthread_t cthread;
  CORE_ADDR symaddr;
  
  symaddr = lookup_address_of_variable ("cproc_list");

  if (! symaddr)
    {
      /* cproc_list is not in a file compiled with debugging
	 symbols, but don't give up yet */

      symaddr = lookup_address_of_variable ("cprocs");

      if (symaddr)
	{
	  static int informed = 0;
	  if (!informed)
	    {
	      informed++;
	      warning ("Your program is loaded with an old threads library.");
	      warning ("GDB does not know the old form of threads");
	      warning ("so things may not work.");
	    }
	}
    }

  /* Stripped or no -lthreads loaded or "cproc_list" is in wrong segment. */
  if (! symaddr)
    return NULL;

  /* Get the address of the first cproc in the task */
  if (!mach3_read_inferior (symaddr,
			    buf,
			    TARGET_PTR_BIT / HOST_CHAR_BIT))
    error ("Can't read cproc master list at address (0x%x).", symaddr);
  their_cprocs = extract_address (buf, TARGET_PTR_BIT / HOST_CHAR_BIT);

  /* Scan the CPROCs in the task.
     CPROCs are chained with LIST field, not NEXT field, which
     chains mutexes, condition variables and queues */

  cproc_head = NULL;

  while (their_cprocs != (CORE_ADDR)0)
    {
      CORE_ADDR cproc_copy_incarnation;
      cproc_copy = (gdb_thread_t) obstack_alloc (cproc_obstack,
						 sizeof (struct gdb_thread));

      if (!mach3_read_inferior (their_cprocs,
				&cproc_copy->raw_cproc[0],
				CPROC_SIZE))
	error("Can't read next cproc at 0x%x.", their_cprocs);

      their_cprocs =
	extract_address (cproc_copy->raw_cproc + CPROC_LIST_OFFSET,
			 CPROC_LIST_SIZE);
      cproc_copy_incarnation =
	extract_address (cproc_copy->raw_cproc + CPROC_INCARNATION_OFFSET,
			 CPROC_INCARNATION_SIZE);

      if (cproc_copy_incarnation == (CORE_ADDR)0)
	cproc_copy->cthread = NULL;
      else
	{
	  /* This CPROC has an attached CTHREAD. Get its name */
	  cthread = (cthread_t)obstack_alloc (cproc_obstack,
					      sizeof(struct cthread));

	  if (!mach3_read_inferior (cproc_copy_incarnation,
				    cthread,
				    sizeof(struct cthread)))
	    error("Can't read next thread at 0x%x.",
		  cproc_copy_incarnation);

	  cproc_copy->cthread = cthread;

	  if (cthread->name)
	    {
	      name = (char *) obstack_alloc (cproc_obstack, MAX_NAME_LEN);

	      if (!mach3_read_inferior(cthread->name, name, MAX_NAME_LEN))
		error("Can't read next thread's name at 0x%x.", cthread->name);

	      cthread->name = name;
	    }
	}

      /* insert in front */
      cproc_copy->next = cproc_head;
      cproc_head = cproc_copy;
    }
  return cproc_head;
}

#ifndef FETCH_CPROC_STATE
/*
 * Check if your machine does not grok the way this routine
 * fetches the FP,PC and SP of a cproc that is not
 * currently attached to any kernel thread (e.g. its cproc.context
 * field points to the place in stack where the context
 * is saved).
 *
 * If it doesn't, define your own routine.
 */
#define FETCH_CPROC_STATE(mth) mach3_cproc_state (mth)

int
mach3_cproc_state (mthread)
     gdb_thread_t mthread;
{
  int context;

  if (! mthread || !mthread->cproc)
    return -1;

  context = extract_signed_integer
    (mthread->cproc->raw_cproc + CPROC_CONTEXT_OFFSET,
     CPROC_CONTEXT_SIZE);
  if (context == 0)
    return -1;

  mthread->sp = context + MACHINE_CPROC_SP_OFFSET;

  if (mach3_read_inferior (context + MACHINE_CPROC_PC_OFFSET,
			   &mthread->pc,
			   sizeof (CORE_ADDR)) != sizeof (CORE_ADDR))
    {
      warning ("Can't read cproc pc from inferior");
      return -1;
    }

  if (mach3_read_inferior (context + MACHINE_CPROC_FP_OFFSET,
			   &mthread->fp,
			   sizeof (CORE_ADDR)) != sizeof (CORE_ADDR))
    {
      warning ("Can't read cproc fp from inferior");
      return -1;
    }

  return 0;
}
#endif /* FETCH_CPROC_STATE */


void
thread_list_command()
{
  thread_basic_info_data_t ths;
  int     thread_count;
  gdb_thread_t cprocs;
  gdb_thread_t scan;
  int     index;
  char   *name;
  char    selected;
  char   *wired;
  int     infoCnt;
  kern_return_t ret;
  mach_port_t   mid_or_port;
  gdb_thread_t  their_threads;
  gdb_thread_t  kthread;

  int neworder = 1;

  char *fmt = "There are %d kernel threads in task %d.\n";
  
  int tmid = map_port_name_to_mid (inferior_task, MACH_TYPE_TASK);
  
  MACH_ERROR_NO_INFERIOR;
  
  thread_count = fetch_thread_info (inferior_task,
				    &their_threads);
  if (thread_count == -1)
    return;
  
  if (thread_count == 1)
    fmt = "There is %d kernel thread in task %d.\n";
  
  printf_filtered (fmt, thread_count, tmid);
  
  puts_filtered (TL_HEADER);
  
  cprocs = get_cprocs();
  
  map_cprocs_to_kernel_threads (cprocs, their_threads, thread_count);
  
  for (scan = cprocs; scan; scan = scan->next)
    {
      int mid;
      char buf[10];
      char slot[3];
      int cproc_state =
	extract_signed_integer
	  (scan->raw_cproc + CPROC_STATE_OFFSET, CPROC_STATE_SIZE);
      
      selected = ' ';
      
      /* a wired cproc? */
      wired = (extract_address (scan->raw_cproc + CPROC_WIRED_OFFSET,
				CPROC_WIRED_SIZE)
	       ? "wired" : "");

      if (scan->reverse_map != -1)
	kthread  = (their_threads + scan->reverse_map);
      else
	kthread  = NULL;

      if (kthread)
	{
	  /* These cprocs have a kernel thread */
	  
	  mid = map_port_name_to_mid (kthread->name, MACH_TYPE_THREAD);
	  
	  infoCnt = THREAD_BASIC_INFO_COUNT;
	  
	  ret = thread_info (kthread->name,
			     THREAD_BASIC_INFO,
			     (thread_info_t)&ths,
			     &infoCnt);
	  
	  if (ret != KERN_SUCCESS)
	    {
	      warning ("Unable to get basic info on thread %d : %s",
		       mid,
		       mach_error_string (ret));
	      continue;
	    }

	  /* Who is the first to have more than 100 threads */
	  sprintf (slot, "%d", kthread->slotid%100);

	  if (kthread->name == current_thread)
	    selected = '*';
	  
	  if (ths.suspend_count)
	    sprintf (buf, "%d", ths.suspend_count);
	  else
	    buf[0] = '\000';

#if 0
	  if (ths.flags & TH_FLAGS_SWAPPED)
	    strcat (buf, "S");
#endif

	  if (ths.flags & TH_FLAGS_IDLE)
	    strcat (buf, "I");

	  printf_filtered (TL_FORMAT,
			   slot,
			   mid,
			   selected,
			   get_thread_name (scan, kthread->slotid),
			   kthread->in_emulator ? "E" : "",
			   translate_state (ths.run_state),
			   buf,
			   translate_cstate (cproc_state),
			   wired);
	  print_tl_address (gdb_stdout, kthread->pc);
	}
      else
	{
	  /* These cprocs don't have a kernel thread.
	   * find out the calling frame with 
	   * FETCH_CPROC_STATE.
	   */

	  struct gdb_thread state;

#if 0
	  /* jtv -> emcmanus: why do you want this here? */
	  if (scan->incarnation == NULL)
	    continue; /* EMcM */
#endif

	  printf_filtered (TL_FORMAT,
			   "-",
			   -neworder,	/* Pseudo MID */
			   selected,
			   get_thread_name (scan, -neworder),
			   "",
			   "-",	/* kernel state */
			   "",
			   translate_cstate (cproc_state),
			   "");
	  state.cproc = scan;

	  if (FETCH_CPROC_STATE (&state) == -1)
	    puts_filtered ("???");
	  else
	    print_tl_address (gdb_stdout, state.pc);

	  neworder++;
	}
      puts_filtered ("\n");
    }
  
  /* Scan for kernel threads without cprocs */
  for (index = 0; index < thread_count; index++)
    {
      if (! their_threads[index].cproc)
	{
	  int mid;
	  
	  char buf[10];
	  char slot[3];

	  mach_port_t name = their_threads[index].name;
	  
	  mid = map_port_name_to_mid (name, MACH_TYPE_THREAD);
	  
	  infoCnt = THREAD_BASIC_INFO_COUNT;
	  
	  ret = thread_info(name,
			    THREAD_BASIC_INFO,
			    (thread_info_t)&ths,
			    &infoCnt);
	    
	  if (ret != KERN_SUCCESS)
	    {
	      warning ("Unable to get basic info on thread %d : %s",
		       mid,
		       mach_error_string (ret));
	      continue;
	    }

	  sprintf (slot, "%d", index%100);

	  if (name == current_thread)
	    selected = '*';
	  else
	    selected = ' ';

	  if (ths.suspend_count)
	    sprintf (buf, "%d", ths.suspend_count);
	  else
	    buf[0] = '\000';

#if 0
	  if (ths.flags & TH_FLAGS_SWAPPED)
	    strcat (buf, "S");
#endif

	  if (ths.flags & TH_FLAGS_IDLE)
	    strcat (buf, "I");

	  printf_filtered (TL_FORMAT,
			   slot,
			   mid,
			   selected,
			   get_thread_name (NULL, index),
			   their_threads[index].in_emulator ? "E" : "",
			   translate_state (ths.run_state),
			   buf,
			   "",   /* No cproc state */
			   "");	/* Can't be wired */
	  print_tl_address (gdb_stdout, their_threads[index].pc);
	  puts_filtered ("\n");
	}
    }
  
  obstack_free (cproc_obstack, 0);
  obstack_init (cproc_obstack);
}

void
thread_select_command(args, from_tty)
     char *args;
     int from_tty;
{
  int mid;
  thread_array_t thread_list;
  int thread_count;
  kern_return_t ret;
  int is_slot = 0;

  MACH_ERROR_NO_INFERIOR;

  if (!args)
    error_no_arg ("MID or @SLOTNUMBER to specify a thread to select");

  while (*args == ' ' || *args == '\t')
    args++;

  if (*args == '@')
    {
      is_slot++;
      args++;
    }

  mid = atoi(args);

  if (mid == 0)
    if (!is_slot || *args != '0') /* Rudimentary checks */
      error ("You must select threads by MID or @SLOTNUMBER");

  if (select_thread (inferior_task, mid, is_slot?2:1) != KERN_SUCCESS)
    return;

  if (from_tty)
    printf_filtered ("Thread %d selected\n",
		     is_slot ? map_port_name_to_mid (current_thread,
						     MACH_TYPE_THREAD) : mid);
}

thread_trace (thread, set)
mach_port_t thread;
boolean_t   set;
{
  int			flavor   = TRACE_FLAVOR;
  unsigned int		stateCnt = TRACE_FLAVOR_SIZE;
  kern_return_t		ret;
  thread_state_data_t	state;

  if (! MACH_PORT_VALID (thread))
    {
      warning ("thread_trace: invalid thread");
      return;
    }

  if (must_suspend_thread)
    setup_thread (thread, 1);

  ret = thread_get_state(thread, flavor, state, &stateCnt);
  CHK ("thread_trace: error reading thread state", ret);
  
  if (set)
    {
      TRACE_SET (thread, state);
    }
  else
    {
      if (! TRACE_CLEAR (thread, state))
	{
	  if (must_suspend_thread)
	    setup_thread (thread, 0);
	  return;
	}
    }

  ret = thread_set_state(thread, flavor, state, stateCnt);
  CHK ("thread_trace: error writing thread state", ret);
  if (must_suspend_thread)
    setup_thread (thread, 0);
}  

#ifdef	FLUSH_INFERIOR_CACHE

/* When over-writing code on some machines the I-Cache must be flushed
   explicitly, because it is not kept coherent by the lazy hardware.
   This definitely includes breakpoints, for instance, or else we
   end up looping in mysterious Bpt traps */

flush_inferior_icache(pc, amount)
     CORE_ADDR pc;
{
  vm_machine_attribute_val_t flush = MATTR_VAL_ICACHE_FLUSH;
  kern_return_t   ret;
  
  ret = vm_machine_attribute (inferior_task,
			      pc,
			      amount,
			      MATTR_CACHE,
			      &flush);
  if (ret != KERN_SUCCESS)
    warning ("Error flushing inferior's cache : %s",
	     mach_error_string (ret));
}
#endif	FLUSH_INFERIOR_CACHE


static
suspend_all_threads (from_tty)
     int from_tty;
{
  kern_return_t	   ret;
  thread_array_t   thread_list;
  int		   thread_count, index;
  int		   infoCnt;
  thread_basic_info_data_t th_info;

  
  ret = task_threads (inferior_task, &thread_list, &thread_count);
  if (ret != KERN_SUCCESS)
    {
      warning ("Could not suspend inferior threads.");
      m3_kill_inferior ();
      return_to_top_level (RETURN_ERROR);
    }
  
  for (index = 0; index < thread_count; index++)
    {
      int mid;

      mid = map_port_name_to_mid (thread_list[ index ],
				  MACH_TYPE_THREAD);
	  
      ret = thread_suspend(thread_list[ index ]);

      if (ret != KERN_SUCCESS)
	warning ("Error trying to suspend thread %d : %s",
		 mid, mach_error_string (ret));

      if (from_tty)
	{
	  infoCnt = THREAD_BASIC_INFO_COUNT;
	  ret = thread_info (thread_list[ index ],
			     THREAD_BASIC_INFO,
			     (thread_info_t) &th_info,
			     &infoCnt);
	  CHK ("suspend can't get thread info", ret);
	  
	  warning ("Thread %d suspend count is %d",
		   mid, th_info.suspend_count);
	}
    }

  consume_send_rights (thread_list, thread_count);
  ret = vm_deallocate(mach_task_self(),
		      (vm_address_t)thread_list, 
		      (thread_count * sizeof(int)));
  CHK ("Error trying to deallocate thread list", ret);
}

void
thread_suspend_command (args, from_tty)
     char *args;
     int from_tty;
{
  kern_return_t ret;
  int           mid;
  mach_port_t   saved_thread;
  int           infoCnt;
  thread_basic_info_data_t th_info;
  
  MACH_ERROR_NO_INFERIOR;

  if (!strcasecmp (args, "all")) {
    suspend_all_threads (from_tty);
    return;
  }

  saved_thread = current_thread;

  mid = parse_thread_id (args, 0, 0);

  if (mid < 0)
    error ("You can suspend only existing kernel threads with MID or @SLOTNUMBER");

  if (mid == 0)
    mid = map_port_name_to_mid (current_thread, MACH_TYPE_THREAD);
  else
    if (select_thread (inferior_task, mid, 0) != KERN_SUCCESS)
      {
	if (current_thread)
	  current_thread = saved_thread;
	error ("Could not select thread %d", mid);
      }

  ret = thread_suspend (current_thread);
  if (ret != KERN_SUCCESS)
    warning ("thread_suspend failed : %s",
	     mach_error_string (ret));

  infoCnt = THREAD_BASIC_INFO_COUNT;
  ret = thread_info (current_thread,
		     THREAD_BASIC_INFO,
		     (thread_info_t) &th_info,
		     &infoCnt);
  CHK ("suspend can't get thread info", ret);
  
  warning ("Thread %d suspend count is %d", mid, th_info.suspend_count);
  
  current_thread = saved_thread;
}

resume_all_threads (from_tty)
     int from_tty;
{
    kern_return_t  ret;
    thread_array_t thread_list;
    int		   thread_count, index;
    int            mid;
    int		   infoCnt;
    thread_basic_info_data_t th_info;

    ret = task_threads (inferior_task, &thread_list, &thread_count);
    if (ret != KERN_SUCCESS)
      {
	m3_kill_inferior ();
	error("task_threads", mach_error_string( ret));
      }

    for (index = 0; index < thread_count; index++)
      {
	infoCnt = THREAD_BASIC_INFO_COUNT;
	ret = thread_info (thread_list [ index ],
			   THREAD_BASIC_INFO,
			   (thread_info_t) &th_info,
			   &infoCnt);
	CHK ("resume_all can't get thread info", ret);
	
	mid = map_port_name_to_mid (thread_list[ index ],
				    MACH_TYPE_THREAD);
	
	if (! th_info.suspend_count)
	  {
	    if (mid != -1 && from_tty)
	      warning ("Thread %d is not suspended", mid);
	    continue;
	  }

	ret = thread_resume (thread_list[ index ]);

	if (ret != KERN_SUCCESS)
	  warning ("Error trying to resume thread %d : %s",
		   mid, mach_error_string (ret));
	else if (mid != -1 && from_tty)
	  warning ("Thread %d suspend count is %d",
		   mid, --th_info.suspend_count);
      }

    consume_send_rights (thread_list, thread_count);
    ret = vm_deallocate(mach_task_self(),
			(vm_address_t)thread_list, 
			(thread_count * sizeof(int)));
    CHK("Error trying to deallocate thread list", ret);
}

void
thread_resume_command (args, from_tty)
     char *args;
     int from_tty;
{
  int mid;
  mach_port_t saved_thread;
  kern_return_t ret;
  thread_basic_info_data_t th_info;
  int infoCnt = THREAD_BASIC_INFO_COUNT;
  
  MACH_ERROR_NO_INFERIOR;

  if (!strcasecmp (args, "all")) {
    resume_all_threads (from_tty);
    return;
  }

  saved_thread = current_thread;

  mid = parse_thread_id (args, 0, 0);

  if (mid < 0)
    error ("You can resume only existing kernel threads with MID or @SLOTNUMBER");

  if (mid == 0)
    mid = map_port_name_to_mid (current_thread, MACH_TYPE_THREAD);
  else
    if (select_thread (inferior_task, mid, 0) != KERN_SUCCESS)
      {
	if (current_thread)
	  current_thread = saved_thread;
	return_to_top_level (RETURN_ERROR);
      }

  ret = thread_info (current_thread,
		     THREAD_BASIC_INFO,
		     (thread_info_t) &th_info,
		     &infoCnt);
  CHK ("resume can't get thread info", ret);
  
  if (! th_info.suspend_count)
    {
      warning ("Thread %d is not suspended", mid);
      goto out;
    }

  ret = thread_resume (current_thread);
  if (ret != KERN_SUCCESS)
    warning ("thread_resume failed : %s",
	     mach_error_string (ret));
  else
    {
      th_info.suspend_count--;
      warning ("Thread %d suspend count is %d", mid, th_info.suspend_count);
    }
      
 out:
  current_thread = saved_thread;
}

void
thread_kill_command (args, from_tty)
     char *args;
     int from_tty;
{
  int mid;
  kern_return_t ret;
  int thread_count;
  thread_array_t thread_table;
  int   index;
  mach_port_t thread_to_kill = MACH_PORT_NULL;
  
  
  MACH_ERROR_NO_INFERIOR;

  if (!args)
    error_no_arg ("thread mid to kill from the inferior task");

  mid = parse_thread_id (args, 0, 0);

  if (mid < 0)
    error ("You can kill only existing kernel threads with MID or @SLOTNUMBER");

  if (mid)
    {
      ret = machid_mach_port (mid_server, mid_auth, mid, &thread_to_kill);
      CHK ("thread_kill_command: machid_mach_port map failed", ret);
    }
  else
    mid = map_port_name_to_mid (current_thread, MACH_TYPE_THREAD);
      
  /* Don't allow gdb to kill *any* thread in the system. Use mkill program for that */
  ret = task_threads (inferior_task, &thread_table, &thread_count);
  CHK ("Error getting inferior's thread list", ret);
  
  if (thread_to_kill == current_thread)
    {
      ret = thread_terminate (thread_to_kill);
      CHK ("Thread could not be terminated", ret);

      if (select_thread (inferior_task, 0, 1) != KERN_SUCCESS)
	warning ("Last thread was killed, use \"kill\" command to kill task");
    }
  else
    for (index = 0; index < thread_count; index++)
      if (thread_table [ index ] == thread_to_kill)
	{
	  ret = thread_terminate (thread_to_kill);
	  CHK ("Thread could not be terminated", ret);
	}

  if (thread_count > 1)
    consume_send_rights (thread_table, thread_count);
  
  ret = vm_deallocate (mach_task_self(), (vm_address_t)thread_table, 
		       (thread_count * sizeof(mach_port_t)));
  CHK ("Error trying to deallocate thread list", ret);
  
  warning ("Thread %d killed", mid);
}


/* Task specific commands; add more if you like */

void
task_resume_command (args, from_tty)
     char *args;
     int from_tty;
{
  kern_return_t ret;
  task_basic_info_data_t ta_info;
  int infoCnt = TASK_BASIC_INFO_COUNT;
  int mid = map_port_name_to_mid (inferior_task, MACH_TYPE_TASK);
  
  MACH_ERROR_NO_INFERIOR;

  /* Would be trivial to change, but is it desirable? */
  if (args)
    error ("Currently gdb can resume only it's inferior task");

  ret = task_info (inferior_task,
		   TASK_BASIC_INFO,
		   (task_info_t) &ta_info,
		   &infoCnt);
  CHK ("task_resume_command: task_info failed", ret);
  
  if (ta_info.suspend_count == 0)
    error ("Inferior task %d is not suspended", mid);
  else if (ta_info.suspend_count == 1 &&
	   from_tty &&
	   !query ("Suspend count is now 1. Do you know what you are doing? "))
    error ("Task not resumed");

  ret = task_resume (inferior_task);
  CHK ("task_resume_command: task_resume", ret);

  if (ta_info.suspend_count == 1)
    {
      warning ("Inferior task %d is no longer suspended", mid);
      must_suspend_thread = 1;
      /* @@ This is not complete: Registers change all the time when not
	 suspended! */
      registers_changed ();
    }
  else
    warning ("Inferior task %d suspend count is now %d",
	     mid, ta_info.suspend_count-1);
}


void
task_suspend_command (args, from_tty)
     char *args;
     int from_tty;
{
  kern_return_t ret;
  task_basic_info_data_t ta_info;
  int infoCnt = TASK_BASIC_INFO_COUNT;
  int mid = map_port_name_to_mid (inferior_task, MACH_TYPE_TASK);
  
  MACH_ERROR_NO_INFERIOR;

  /* Would be trivial to change, but is it desirable? */
  if (args)
    error ("Currently gdb can suspend only it's inferior task");

  ret = task_suspend (inferior_task);
  CHK ("task_suspend_command: task_suspend", ret);

  must_suspend_thread = 0;

  ret = task_info (inferior_task,
		   TASK_BASIC_INFO,
		   (task_info_t) &ta_info,
		   &infoCnt);
  CHK ("task_suspend_command: task_info failed", ret);
  
  warning ("Inferior task %d suspend count is now %d",
	   mid, ta_info.suspend_count);
}

static char *
get_size (bytes)
     int bytes;
{
  static char size [ 30 ];
  int zz = bytes/1024;

  if (zz / 1024)
    sprintf (size, "%-2.1f M", ((float)bytes)/(1024.0*1024.0));
  else
    sprintf (size, "%d K", zz);

  return size;
}

/* Does this require the target task to be suspended?? I don't think so. */
void
task_info_command (args, from_tty)
     char *args;
     int from_tty;
{
  int mid = -5;
  mach_port_t task;
  kern_return_t ret;
  task_basic_info_data_t ta_info;
  int infoCnt = TASK_BASIC_INFO_COUNT;
  int page_size = round_page(1);
  int thread_count = 0;
  
  if (MACH_PORT_VALID (inferior_task))
    mid = map_port_name_to_mid (inferior_task,
				MACH_TYPE_TASK);

  task = inferior_task;

  if (args)
    {
      int tmid = atoi (args);

      if (tmid <= 0)
	error ("Invalid mid %d for task info", tmid);

      if (tmid != mid)
	{
	  mid = tmid;
	  ret = machid_mach_port (mid_server, mid_auth, tmid, &task);
	  CHK ("task_info_command: machid_mach_port map failed", ret);
	}
    }

  if (mid < 0)
    error ("You have to give the task MID as an argument");

  ret = task_info (task,
		   TASK_BASIC_INFO,
		   (task_info_t) &ta_info,
		   &infoCnt);
  CHK ("task_info_command: task_info failed", ret);

  printf_filtered ("\nTask info for task %d:\n\n", mid);
  printf_filtered (" Suspend count : %d\n", ta_info.suspend_count);
  printf_filtered (" Base priority : %d\n", ta_info.base_priority);
  printf_filtered (" Virtual size  : %s\n", get_size (ta_info.virtual_size));
  printf_filtered (" Resident size : %s\n", get_size (ta_info.resident_size));

  {
    thread_array_t thread_list;
    
    ret = task_threads (task, &thread_list, &thread_count);
    CHK ("task_info_command: task_threads", ret);
    
    printf_filtered (" Thread count  : %d\n", thread_count);

    consume_send_rights (thread_list, thread_count);
    ret = vm_deallocate(mach_task_self(),
			(vm_address_t)thread_list, 
			(thread_count * sizeof(int)));
    CHK("Error trying to deallocate thread list", ret);
  }
  if (have_emulator_p (task))
    printf_filtered (" Emulator at   : 0x%x..0x%x\n",
		     EMULATOR_BASE, EMULATOR_END);
  else
    printf_filtered (" No emulator.\n");

  if (thread_count && task == inferior_task)
    printf_filtered ("\nUse the \"thread list\" command to see the threads\n");
}

/* You may either FORWARD the exception to the inferior, or KEEP
 * it and return to GDB command level.
 *
 * exception mid [ forward | keep ]
 */

static void
exception_command (args, from_tty)
     char *args;
     int from_tty;
{
  char *scan = args;
  int exception;
  int len;

  if (!args)
    error_no_arg ("exception number action");

  while (*scan == ' ' || *scan == '\t') scan++;
  
  if ('0' <= *scan && *scan <= '9')
    while ('0' <= *scan && *scan <= '9')
      scan++;
  else
    error ("exception number action");

  exception = atoi (args);
  if (exception <= 0 || exception > MAX_EXCEPTION)
    error ("Allowed exception numbers are in range 1..%d",
	   MAX_EXCEPTION);

  if (*scan != ' ' && *scan != '\t')
    error ("exception number must be followed by a space");
  else
    while (*scan == ' ' || *scan == '\t') scan++;

  args = scan;
  len = 0;
  while (*scan)
    {
      len++;
      scan++;
    }

  if (!len)
    error("exception number action");

  if (!strncasecmp (args, "forward", len))
    exception_map[ exception ].forward = TRUE;
  else if (!strncasecmp (args, "keep", len))
    exception_map[ exception ].forward = FALSE;
  else
    error ("exception action is either \"keep\" or \"forward\"");
}

static void
print_exception_info (exception)
     int exception;
{
  boolean_t forward = exception_map[ exception ].forward;

  printf_filtered ("%s\t(%d): ", exception_map[ exception ].name,
		   exception);
  if (!forward)
    if (exception_map[ exception ].sigmap != SIG_UNKNOWN)
      printf_filtered ("keep and handle as signal %d\n",
		       exception_map[ exception ].sigmap);
    else
      printf_filtered ("keep and handle as unknown signal %d\n",
		       exception_map[ exception ].sigmap);
  else
    printf_filtered ("forward exception to inferior\n");
}

void
exception_info (args, from_tty)
     char *args;
     int from_tty;
{
  int exception;

  if (!args)
    for (exception = 1; exception <= MAX_EXCEPTION; exception++)
      print_exception_info (exception);
  else
    {
      exception = atoi (args);

      if (exception <= 0 || exception > MAX_EXCEPTION)
	error ("Invalid exception number, values from 1 to %d allowed",
	       MAX_EXCEPTION);
      print_exception_info (exception);
    }
}

/* Check for actions for mach exceptions.
 */
mach3_exception_actions (w, force_print_only, who)
     WAITTYPE *w;
     boolean_t force_print_only;
     char *who;
{
  boolean_t force_print = FALSE;

  
  if (force_print_only ||
      exception_map[stop_exception].sigmap == SIG_UNKNOWN)
    force_print = TRUE;
  else
    WSETSTOP (*w, exception_map[stop_exception].sigmap);

  if (exception_map[stop_exception].print || force_print)
    {
      target_terminal_ours ();
      
      printf_filtered ("\n%s received %s exception : ",
		       who,
		       exception_map[stop_exception].name);
      
      wrap_here ("   ");

      switch(stop_exception) {
      case EXC_BAD_ACCESS:
	printf_filtered ("referencing address 0x%x : %s\n",
			 stop_subcode,
			 mach_error_string (stop_code));
	break;
      case EXC_BAD_INSTRUCTION:
	printf_filtered
	  ("illegal or undefined instruction. code %d subcode %d\n",
	   stop_code, stop_subcode);
	break;
      case EXC_ARITHMETIC:
	printf_filtered ("code %d\n", stop_code);
	break;
      case EXC_EMULATION:
	printf_filtered ("code %d subcode %d\n", stop_code, stop_subcode);
	break;
      case EXC_SOFTWARE:
	printf_filtered ("%s specific, code 0x%x\n",
			 stop_code < 0xffff ? "hardware" : "os emulation",
			 stop_code);
	break;
      case EXC_BREAKPOINT:
	printf_filtered ("type %d (machine dependent)\n",
			 stop_code);
	break;
      default:
	fatal ("Unknown exception");
      }
    }
}

setup_notify_port (create_new)
     int create_new;
{
  kern_return_t ret;

  if (MACH_PORT_VALID (our_notify_port))
    {
      ret = mach_port_destroy (mach_task_self (), our_notify_port);
      CHK ("Could not destroy our_notify_port", ret);
    }

  our_notify_port = MACH_PORT_NULL;
  notify_chain    = (port_chain_t) NULL;
  port_chain_destroy (port_chain_obstack);

  if (create_new)
    {
      ret = mach_port_allocate (mach_task_self(),
				MACH_PORT_RIGHT_RECEIVE,
				&our_notify_port);
      if (ret != KERN_SUCCESS)
	fatal("Creating notify port %s", mach_error_string(ret));
      
      ret = mach_port_move_member(mach_task_self(), 
				  our_notify_port,
				  inferior_wait_port_set);
      if (ret != KERN_SUCCESS)
	fatal("initial move member %s",mach_error_string(ret));
    }
}

/*
 * Register our message port to the net name server
 *
 * Currently used only by the external stop-gdb program
 * since ^C does not work if you would like to enter
 * gdb command level while debugging your program.
 *
 * NOTE: If the message port is sometimes used for other
 * purposes also, the NAME must not be a guessable one.
 * Then, there should be a way to change it.
 */

char registered_name[ MAX_NAME_LEN ];

void
message_port_info (args, from_tty)
     char *args;
     int from_tty;
{
  if (registered_name[0])
    printf_filtered ("gdb's message port name: '%s'\n",
		     registered_name);
  else
    printf_filtered ("gdb's message port is not currently registered\n");
}

void
gdb_register_port (name, port)
     char *name;
     mach_port_t port;
{
  kern_return_t ret;
  static int already_signed = 0;
  int len;

  if (! MACH_PORT_VALID (port) || !name || !*name)
    {
      warning ("Invalid registration request");
      return;
    }

  if (! already_signed)
    {
      ret = mach_port_insert_right (mach_task_self (),
				    our_message_port,
				    our_message_port,
				    MACH_MSG_TYPE_MAKE_SEND);
      CHK ("Failed to create a signature to our_message_port", ret);
      already_signed = 1;
    }
  else if (already_signed > 1)
    {
      ret = netname_check_out (name_server_port,
			       registered_name,
			       our_message_port);
      CHK ("Failed to check out gdb's message port", ret);
      registered_name[0] = '\000';
      already_signed = 1;
    }

  ret = netname_check_in (name_server_port,	/* Name server port */
			  name,			/* Name of service */
			  our_message_port,	/* Signature */
			  port); 		/* Creates a new send right */
  CHK("Failed to check in the port", ret);
  
  len = 0;
  while(len < MAX_NAME_LEN && *(name+len))
    {
      registered_name[len] = *(name+len);
      len++;
    }
  registered_name[len] = '\000';
  already_signed = 2;
}  

struct cmd_list_element *cmd_thread_list;
struct cmd_list_element *cmd_task_list;

/*ARGSUSED*/
static void
thread_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  printf_unfiltered ("\"thread\" must be followed by the name of a thread command.\n");
  help_list (cmd_thread_list, "thread ", -1, gdb_stdout);
}

/*ARGSUSED*/
static void
task_command (arg, from_tty)
     char *arg;
     int from_tty;
{
  printf_unfiltered ("\"task\" must be followed by the name of a task command.\n");
  help_list (cmd_task_list, "task ", -1, gdb_stdout);
}

add_mach_specific_commands ()
{
  /* Thread handling commands */

  /* FIXME: Move our thread support into the generic thread.c stuff so we
     can share that code.  */
  add_prefix_cmd ("mthread", class_stack, thread_command,
      "Generic command for handling Mach threads in the debugged task.",
      &cmd_thread_list, "thread ", 0, &cmdlist);

  add_com_alias ("th", "mthread", class_stack, 1);

  add_cmd ("select", class_stack, thread_select_command, 
	   "Select and print MID of the selected thread",
	   &cmd_thread_list);
  add_cmd ("list",   class_stack, thread_list_command,
	   "List info of task's threads. Selected thread is marked with '*'",
	   &cmd_thread_list);
  add_cmd ("suspend", class_run, thread_suspend_command,
	   "Suspend one or all of the threads in the selected task.",
	   &cmd_thread_list);
  add_cmd ("resume", class_run, thread_resume_command,
	   "Resume one or all of the threads in the selected task.",
	   &cmd_thread_list);
  add_cmd ("kill", class_run, thread_kill_command,
	   "Kill the specified thread MID from inferior task.",
	   &cmd_thread_list);
#if 0
  /* The rest of this support (condition_thread) was not merged.  It probably
     should not be merged in this form, but instead added to the generic GDB
     thread support.  */
  add_cmd ("break", class_breakpoint, condition_thread,
	   "Breakpoint N will only be effective for thread MID or @SLOT\n\
	    If MID/@SLOT is omitted allow all threads to break at breakpoint",
	   &cmd_thread_list);
#endif
  /* Thread command shorthands (for backward compatibility) */
  add_alias_cmd ("ts", "mthread select", 0, 0, &cmdlist);
  add_alias_cmd ("tl", "mthread list",   0, 0, &cmdlist);

  /* task handling commands */

  add_prefix_cmd ("task", class_stack, task_command,
      "Generic command for handling debugged task.",
      &cmd_task_list, "task ", 0, &cmdlist);

  add_com_alias ("ta", "task", class_stack, 1);

  add_cmd ("suspend", class_run, task_suspend_command,
	   "Suspend the inferior task.",
	   &cmd_task_list);
  add_cmd ("resume", class_run, task_resume_command,
	   "Resume the inferior task.",
	   &cmd_task_list);
  add_cmd ("info", no_class, task_info_command,
	   "Print information about the specified task.",
	   &cmd_task_list);

  /* Print my message port name */

  add_info ("message-port", message_port_info,
	    "Returns the name of gdb's message port in the netnameserver");

  /* Exception commands */

  add_info ("exceptions", exception_info,
	    "What debugger does when program gets various exceptions.\n\
Specify an exception number as argument to print info on that\n\
exception only.");

  add_com ("exception", class_run, exception_command,
	   "Specify how to handle an exception.\n\
Args are exception number followed by \"forward\" or \"keep\".\n\
`Forward' means forward the exception to the program's normal exception\n\
handler.\n\
`Keep' means reenter debugger if this exception happens, and GDB maps\n\
the exception to some signal (see info exception)\n\
Normally \"keep\" is used to return to GDB on exception.");
}

kern_return_t
do_mach_notify_dead_name (notify, name)
     mach_port_t notify;
     mach_port_t name;
{
  kern_return_t kr = KERN_SUCCESS;

  /* Find the thing that notified */
  port_chain_t element = port_chain_member (notify_chain, name);

  /* Take name of from unreceived dead name notification list */
  notify_chain = port_chain_delete (notify_chain, name);

  if (! element)
    error ("Received a dead name notify from unchained port (0x%x)", name);
  
  switch (element->type) {

  case MACH_TYPE_THREAD:
    target_terminal_ours_for_output ();
    if (name == current_thread)
      {
	printf_filtered ("\nCurrent thread %d died", element->mid);
	current_thread = MACH_PORT_NULL;
      }
    else
      printf_filtered ("\nThread %d died", element->mid);

    break;

  case MACH_TYPE_TASK:
    target_terminal_ours_for_output ();
    if (name != inferior_task)
      printf_filtered ("Task %d died, but it was not the selected task",
	       element->mid);
    else	       
      {
	printf_filtered ("Current task %d died", element->mid);
	
	mach_port_destroy (mach_task_self(), name);
	inferior_task = MACH_PORT_NULL;
	
	if (notify_chain)
	  warning ("There were still unreceived dead_name_notifications???");
	
	/* Destroy the old notifications */
	setup_notify_port (0);

      }
    break;

  default:
    error ("Unregistered dead_name 0x%x notification received. Type is %d, mid is 0x%x",
	   name, element->type, element->mid);
    break;
  }

  return KERN_SUCCESS;
}

kern_return_t
do_mach_notify_msg_accepted (notify, name)
     mach_port_t notify;
     mach_port_t name;
{
  warning ("do_mach_notify_msg_accepted : notify %x, name %x",
	   notify, name);
  return KERN_SUCCESS;
}

kern_return_t
do_mach_notify_no_senders (notify, mscount)
     mach_port_t notify;
     mach_port_mscount_t mscount;
{
  warning ("do_mach_notify_no_senders : notify %x, mscount %x",
	   notify, mscount);
  return KERN_SUCCESS;
}

kern_return_t
do_mach_notify_port_deleted (notify, name)
     mach_port_t notify;
     mach_port_t name;
{
  warning ("do_mach_notify_port_deleted : notify %x, name %x",
	   notify, name);
  return KERN_SUCCESS;
}

kern_return_t
do_mach_notify_port_destroyed (notify, rights)
     mach_port_t notify;
     mach_port_t rights;
{
  warning ("do_mach_notify_port_destroyed : notify %x, rights %x",
	   notify, rights);
  return KERN_SUCCESS;
}

kern_return_t
do_mach_notify_send_once (notify)
     mach_port_t notify;
{
#ifdef DUMP_SYSCALL
  /* MANY of these are generated. */
  warning ("do_mach_notify_send_once : notify %x",
	   notify);
#endif
  return KERN_SUCCESS;
}

/* Kills the inferior. It's gone when you call this */
static void
kill_inferior_fast ()
{
  WAITTYPE w;

  if (inferior_pid == 0 || inferior_pid == 1)
    return;

  /* kill() it, since the Unix server does not otherwise notice when
   * killed with task_terminate().
   */
  if (inferior_pid > 0)
    kill (inferior_pid, SIGKILL);

  /* It's propably terminate already */
  (void) task_terminate (inferior_task);

  inferior_task  = MACH_PORT_NULL;
  current_thread = MACH_PORT_NULL;

  wait3 (&w, WNOHANG, 0);

  setup_notify_port (0);
}

static void
m3_kill_inferior ()
{
  kill_inferior_fast ();
  target_mourn_inferior ();
}

/* Clean up after the inferior dies.  */

static void
m3_mourn_inferior ()
{
  unpush_target (&m3_ops);
  generic_mourn_inferior ();
}


/* Fork an inferior process, and start debugging it.  */

static void
m3_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  fork_inferior (exec_file, allargs, env, m3_trace_me, m3_trace_him, NULL);
  /* We are at the first instruction we care about.  */
  /* Pedal to the metal... */
  proceed ((CORE_ADDR) -1, 0, 0);
}

/* Mark our target-struct as eligible for stray "run" and "attach"
   commands.  */
static int
m3_can_run ()
{
  return 1;
}

/* Mach 3.0 does not need ptrace for anything
 * Make sure nobody uses it on mach.
 */
ptrace (a,b,c,d)
int a,b,c,d;
{
  error ("Lose, Lose! Somebody called ptrace\n");
}

/* Resume execution of the inferior process.
   If STEP is nonzero, single-step it.
   If SIGNAL is nonzero, give it that signal.  */

void
m3_resume (pid, step, signal)
     int pid;
     int step;
     enum target_signal signal;
{
  kern_return_t	ret;

  if (step)
    {
      thread_basic_info_data_t th_info;
      unsigned int	       infoCnt = THREAD_BASIC_INFO_COUNT;
      
      /* There is no point in single stepping when current_thread
       * is dead.
       */
      if (! MACH_PORT_VALID (current_thread))
	error ("No thread selected; can not single step");
      
      /* If current_thread is suspended, tracing it would never return.
       */
      ret = thread_info (current_thread,
			 THREAD_BASIC_INFO,
			 (thread_info_t) &th_info,
			 &infoCnt);
      CHK ("child_resume: can't get thread info", ret);
      
      if (th_info.suspend_count)
	error ("Can't trace a suspended thread. Use \"thread resume\" command to resume it");
    }

  vm_read_cache_valid = FALSE;

  if (signal && inferior_pid > 0) /* Do not signal, if attached by MID */
    kill (inferior_pid, target_signal_to_host (signal));

  if (step)
    {
      suspend_all_threads (0);

      setup_single_step (current_thread, TRUE);
      
      ret = thread_resume (current_thread);
      CHK ("thread_resume", ret);
    }
  
  ret = task_resume (inferior_task);
  if (ret == KERN_FAILURE)
    warning ("Task was not suspended");
  else
    CHK ("Resuming task", ret);
  
  /* HACK HACK This is needed by the multiserver system HACK HACK */
  while ((ret = task_resume(inferior_task)) == KERN_SUCCESS)
    /* make sure it really runs */;
  /* HACK HACK This is needed by the multiserver system HACK HACK */
}

#ifdef ATTACH_DETACH

/* Start debugging the process with the given task */
void
task_attach (tid)
  task_t tid;
{
  kern_return_t ret;
  inferior_task = tid;

  ret = task_suspend (inferior_task);
  CHK("task_attach: task_suspend", ret);

  must_suspend_thread = 0;

  setup_notify_port (1);

  request_notify (inferior_task, MACH_NOTIFY_DEAD_NAME, MACH_TYPE_TASK);

  setup_exception_port ();
  
  emulator_present = have_emulator_p (inferior_task);

  attach_flag = 1;
}

/* Well, we can call error also here and leave the
 * target stack inconsistent. Sigh.
 * Fix this sometime (the only way to fail here is that
 * the task has no threads at all, which is rare, but
 * possible; or if the target task has died, which is also
 * possible, but unlikely, since it has been suspended.
 * (Someone must have killed it))
 */
void
attach_to_thread ()
{
  if (select_thread (inferior_task, 0, 1) != KERN_SUCCESS)
    error ("Could not select any threads to attach to");
}

mid_attach (mid)
    int	mid;
{
    kern_return_t ret;

    ret = machid_mach_port (mid_server, mid_auth, mid, &inferior_task);
    CHK("mid_attach: machid_mach_port", ret);

    task_attach (inferior_task);

    return mid;
}

/* 
 * Start debugging the process whose unix process-id is PID.
 * A negative "pid" value is legal and signifies a mach_id not a unix pid.
 *
 * Prevent (possible unwanted) dangerous operations by enabled users
 * like "atta 0" or "atta foo" (equal to the previous :-) and
 * "atta pidself". Anyway, the latter is allowed by specifying a MID.
 */
static int
m3_do_attach (pid)
     int pid;
{
  kern_return_t ret;

  if (pid == 0)
    error("MID=0, Debugging the master unix server does not compute");

  /* Foo. This assumes gdb has a unix pid */
  if (pid == getpid())
    error ("I will debug myself only by mid. (Gdb would suspend itself!)");

  if (pid < 0)
    {
      mid_attach (-(pid));

      /* inferior_pid will be NEGATIVE! */
      inferior_pid = pid;

      return inferior_pid;
    }

  inferior_task = task_by_pid (pid);
  if (! MACH_PORT_VALID (inferior_task))
    error("Cannot map Unix pid %d to Mach task port", pid);

  task_attach (inferior_task);

  inferior_pid = pid;

  return inferior_pid;
}

/* Attach to process PID, then initialize for debugging it
   and wait for the trace-trap that results from attaching.  */

static void
m3_attach (args, from_tty)
     char *args;
     int from_tty;
{
  char *exec_file;
  int pid;

  if (!args)
    error_no_arg ("process-id to attach");

  pid = atoi (args);

  if (pid == getpid())		/* Trying to masturbate? */
    error ("I refuse to debug myself!");

  if (from_tty)
    {
      exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf_unfiltered ("Attaching to program `%s', %s\n", exec_file, target_pid_to_str (pid));
      else
	printf_unfiltered ("Attaching to %s\n", target_pid_to_str (pid));

      gdb_flush (gdb_stdout);
    }

  m3_do_attach (pid);
  inferior_pid = pid;
  push_target (&m3_ops);
}

void
deallocate_inferior_ports ()
{
  kern_return_t  ret;
  thread_array_t thread_list;
  int		 thread_count, index;

  if (!MACH_PORT_VALID (inferior_task))
    return;

  ret = task_threads (inferior_task, &thread_list, &thread_count);
  if (ret != KERN_SUCCESS)
    {
      warning ("deallocate_inferior_ports: task_threads",
	       mach_error_string(ret));
      return;
    }

  /* Get rid of send rights to task threads */
  for (index = 0; index < thread_count; index++)
    {
      int rights;
      ret = mach_port_get_refs (mach_task_self (),
				thread_list[index],
				MACH_PORT_RIGHT_SEND,
				&rights);
      CHK("deallocate_inferior_ports: get refs", ret);

      if (rights > 0)
	{
	  ret = mach_port_mod_refs (mach_task_self (),
				    thread_list[index],
				    MACH_PORT_RIGHT_SEND,
				    -rights);
	  CHK("deallocate_inferior_ports: mod refs", ret);
	}
    }

  ret = mach_port_mod_refs (mach_task_self (),
			    inferior_exception_port,
			    MACH_PORT_RIGHT_RECEIVE,
			    -1);
  CHK ("deallocate_inferior_ports: cannot get rid of exception port", ret);

  ret = mach_port_deallocate (mach_task_self (),
			      inferior_task);
  CHK ("deallocate_task_port: deallocating inferior_task", ret);

  current_thread = MACH_PORT_NULL;
  inferior_task  = MACH_PORT_NULL;
}

/* Stop debugging the process whose number is PID
   and continue it with signal number SIGNAL.
   SIGNAL = 0 means just continue it.  */

static void
m3_do_detach (signal)
     int signal;
{
  kern_return_t ret;

  MACH_ERROR_NO_INFERIOR;

  if (current_thread != MACH_PORT_NULL)
    {
      /* Store the gdb's view of the thread we are deselecting
       * before we detach.
       * @@ I am really not sure if this is ever needeed.
       */
      target_prepare_to_store ();
      target_store_registers (-1);
    }

  ret = task_set_special_port (inferior_task,
			       TASK_EXCEPTION_PORT, 
			       inferior_old_exception_port);
  CHK ("task_set_special_port", ret);

  /* Discard all requested notifications */
  setup_notify_port (0);

  if (remove_breakpoints ())
    warning ("Could not remove breakpoints when detaching");
  
  if (signal && inferior_pid > 0)
    kill (inferior_pid, signal);
  
  /* the task might be dead by now */
  (void) task_resume (inferior_task);
  
  deallocate_inferior_ports ();
  
  attach_flag = 0;
}

/* Take a program previously attached to and detaches it.
   The program resumes execution and will no longer stop
   on signals, etc.  We'd better not have left any breakpoints
   in the program or it'll die when it hits one.  For this
   to work, it may be necessary for the process to have been
   previously attached.  It *might* work if the program was
   started via fork.  */

static void
m3_detach (args, from_tty)
     char *args;
     int from_tty;
{
  int siggnal = 0;

  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf_unfiltered ("Detaching from program: %s %s\n",
	      exec_file, target_pid_to_str (inferior_pid));
      gdb_flush (gdb_stdout);
    }
  if (args)
    siggnal = atoi (args);
  
  m3_do_detach (siggnal);
  inferior_pid = 0;
  unpush_target (&m3_ops);		/* Pop out of handling an inferior */
}
#endif /* ATTACH_DETACH */

/* Get ready to modify the registers array.  On machines which store
   individual registers, this doesn't need to do anything.  On machines
   which store all the registers in one fell swoop, this makes sure
   that registers contains all the registers from the program being
   debugged.  */

static void
m3_prepare_to_store ()
{
#ifdef CHILD_PREPARE_TO_STORE
  CHILD_PREPARE_TO_STORE ();
#endif
}

/* Print status information about what we're accessing.  */

static void
m3_files_info (ignore)
     struct target_ops *ignore;
{
  /* FIXME: should print MID and all that crap.  */
  printf_unfiltered ("\tUsing the running image of %s %s.\n",
		       attach_flag? "attached": "child", target_pid_to_str (inferior_pid));
}

static void
m3_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  error ("Use the \"run\" command to start a Unix child process.");
}

#ifdef DUMP_SYSCALL
#ifdef __STDC__
#define STR(x) #x
#else
#define STR(x) "x"
#endif

char	*bsd1_names[] = {
  "execve",
  "fork",
  "take_signal",
  "sigreturn",
  "getrusage",
  "chdir",
  "chroot",
  "open",
  "creat",
  "mknod",
  "link",
  "symlink",
  "unlink",
  "access",
  "stat",
  "readlink",
  "chmod",
  "chown",
  "utimes",
  "truncate",
  "rename",
  "mkdir",
  "rmdir",
  "xutimes",
  "mount",
  "umount",
  "acct",
  "setquota",
  "write_short",
  "write_long",
  "send_short",
  "send_long",
  "sendto_short",
  "sendto_long",
  "select",
  "task_by_pid",
  "recvfrom_short",
  "recvfrom_long",
  "setgroups",
  "setrlimit",
  "sigvec",
  "sigstack",
  "settimeofday",
  "adjtime",
  "setitimer",
  "sethostname",
  "bind",
  "accept",
  "connect",
  "setsockopt",
  "getsockopt",
  "getsockname",
  "getpeername",
  "init_process",
  "table_set",
  "table_get",
  "pioctl",
  "emulator_error",
  "readwrite",
  "share_wakeup",
  0,
  "maprw_request_it",
  "maprw_release_it",
  "maprw_remap",
  "pid_by_task",
};

int	bsd1_nnames = sizeof(bsd1_names)/sizeof(bsd1_names[0]);

char*
name_str(name,buf)

int	name;
char	*buf;

{
  switch (name) {
  case MACH_MSG_TYPE_BOOLEAN:
    return "boolean";
  case MACH_MSG_TYPE_INTEGER_16:
    return "short";
  case MACH_MSG_TYPE_INTEGER_32:
    return "long";
  case MACH_MSG_TYPE_CHAR:
    return "char";
  case MACH_MSG_TYPE_BYTE:
    return "byte";
  case MACH_MSG_TYPE_REAL:
    return "real";
  case MACH_MSG_TYPE_STRING:
    return "string";
  default:
    sprintf(buf,"%d",name);
    return buf;
  }
}

char *
id_str(id,buf)

int	id;
char	*buf;

{
  char	*p;
  if (id >= 101000 && id < 101000+bsd1_nnames) {
    if (p = bsd1_names[id-101000])
      return p;
  }
  if (id == 102000)
    return "psignal_retry";
  if (id == 100000)
    return "syscall";
  sprintf(buf,"%d",id);
  return buf;
}

print_msg(mp)
mach_msg_header_t	*mp;
{
  char	*fmt_x = "%20s : 0x%08x\n";
  char	*fmt_d = "%20s : %10d\n";
  char	*fmt_s = "%20s : %s\n";
  char	buf[100];

  puts_filtered ("\n");
#define pr(fmt,h,x) printf_filtered(fmt,STR(x),(h).x)
  pr(fmt_x,(*mp),msgh_bits);
  pr(fmt_d,(*mp),msgh_size);
  pr(fmt_x,(*mp),msgh_remote_port);
  pr(fmt_x,(*mp),msgh_local_port);
  pr(fmt_d,(*mp),msgh_kind);
  printf_filtered(fmt_s,STR(msgh_id),id_str(mp->msgh_id,buf));
  
  if (debug_level > 1)
  {
    char	*p,*ep,*dp;
    int		plen;
    p = (char*)mp;
    ep = p+mp->msgh_size;
    p += sizeof(*mp);
    for(; p < ep; p += plen) {
      mach_msg_type_t	*tp;
      mach_msg_type_long_t	*tlp;
      int	name,size,number;
      tp = (mach_msg_type_t*)p;
      if (tp->msgt_longform) {
	tlp = (mach_msg_type_long_t*)tp;
	name = tlp->msgtl_name;
	size = tlp->msgtl_size;
	number = tlp->msgtl_number;
	plen = sizeof(*tlp);
      } else {
	name = tp->msgt_name;
	size = tp->msgt_size;
	number = tp->msgt_number;
	plen = sizeof(*tp);
      }
      printf_filtered("name=%-16s size=%2d number=%7d inline=%d long=%d deal=%d\n",
		      name_str(name,buf),size,number,tp->msgt_inline,
		      tp->msgt_longform, tp->msgt_deallocate);
      dp = p+plen;
      if (tp->msgt_inline) {
	int	l;
	l = size*number/8;
	l = (l+sizeof(long)-1)&~((sizeof(long))-1);
	plen += l;
	print_data(dp,size,number);
      } else {
	plen += sizeof(int*);
      }
      printf_filtered("plen=%d\n",plen);
    }
  }
}

print_data(p,size,number)

char	*p;

{
  int	*ip;
  short	*sp;
  int	i;

  switch (size) {
  case 8:
    for(i = 0; i < number; i++) {
      printf_filtered(" %02x",p[i]);
    }
    break;
  case 16:
    sp = (short*)p;
    for(i = 0; i < number; i++) {
      printf_filtered(" %04x",sp[i]);
    }
    break;
  case 32:
    ip = (int*)p;
    for(i = 0; i < number; i++) {
      printf_filtered(" %08x",ip[i]);
    }
    break;
  }
  puts_filtered("\n");
}
#endif  DUMP_SYSCALL

static void
m3_stop ()
{
  error ("to_stop target function not implemented");
}

struct target_ops m3_ops = {
  "mach",			/* to_shortname */
  "Mach child process",	/* to_longname */
  "Mach child process (started by the \"run\" command).",	/* to_doc */
  m3_open,			/* to_open */
  0,				/* to_close */
  m3_attach,			/* to_attach */
  m3_detach, 		/* to_detach */
  m3_resume,			/* to_resume */
  mach_really_wait,			/* to_wait */
  fetch_inferior_registers,	/* to_fetch_registers */
  store_inferior_registers,	/* to_store_registers */
  m3_prepare_to_store,	/* to_prepare_to_store */
  m3_xfer_memory,		/* to_xfer_memory */
  m3_files_info,		/* to_files_info */
  memory_insert_breakpoint,	/* to_insert_breakpoint */
  memory_remove_breakpoint,	/* to_remove_breakpoint */
  terminal_init_inferior,	/* to_terminal_init */
  terminal_inferior, 		/* to_terminal_inferior */
  terminal_ours_for_output,	/* to_terminal_ours_for_output */
  terminal_ours,		/* to_terminal_ours */
  child_terminal_info,		/* to_terminal_info */
  m3_kill_inferior,		/* to_kill */
  0,				/* to_load */
  0,				/* to_lookup_symbol */

  m3_create_inferior,	/* to_create_inferior */
  m3_mourn_inferior,	/* to_mourn_inferior */
  m3_can_run,		/* to_can_run */
  0,				/* to_notice_signals */
  0,				/* to_thread_alive */
  m3_stop,			/* to_stop */
  process_stratum,		/* to_stratum */
  0,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  0,				/* sections */
  0,				/* sections_end */
  OPS_MAGIC			/* to_magic */
};

void
_initialize_m3_nat ()
{
  kern_return_t ret;

  add_target (&m3_ops);

  ret = mach_port_allocate(mach_task_self(), 
			   MACH_PORT_RIGHT_PORT_SET,
			   &inferior_wait_port_set);
  if (ret != KERN_SUCCESS)
    fatal("initial port set %s",mach_error_string(ret));

  /* mach_really_wait now waits for this */
  currently_waiting_for = inferior_wait_port_set;

  ret = netname_look_up(name_server_port, hostname, "MachID", &mid_server);
  if (ret != KERN_SUCCESS)
    {
      mid_server = MACH_PORT_NULL;
      
      warning ("initialize machid: netname_lookup_up(MachID) : %s",
	       mach_error_string(ret));
      warning ("Some (most?) features disabled...");
    }
  
  mid_auth = mach_privileged_host_port();
  if (mid_auth == MACH_PORT_NULL)
    mid_auth = mach_task_self();
  
  obstack_init (port_chain_obstack);

  ret = mach_port_allocate (mach_task_self (), 
			    MACH_PORT_RIGHT_RECEIVE,
			    &thread_exception_port);
  CHK ("Creating thread_exception_port for single stepping", ret);
  
  ret = mach_port_insert_right (mach_task_self (),
				thread_exception_port,
				thread_exception_port,
				MACH_MSG_TYPE_MAKE_SEND);
  CHK ("Inserting send right to thread_exception_port", ret);

  /* Allocate message port */
  ret = mach_port_allocate (mach_task_self (),
			    MACH_PORT_RIGHT_RECEIVE,
			    &our_message_port);
  if (ret != KERN_SUCCESS)
    warning ("Creating message port %s", mach_error_string (ret));
  else
    {
      char buf[ MAX_NAME_LEN ];
      ret = mach_port_move_member(mach_task_self (),
				  our_message_port,
				  inferior_wait_port_set);
      if (ret != KERN_SUCCESS)
	warning ("message move member %s", mach_error_string (ret));


      /* @@@@ No way to change message port name currently */
      /* Foo. This assumes gdb has a unix pid */
      sprintf (buf, "gdb-%d", getpid ());
      gdb_register_port (buf, our_message_port);
    }
  
  /* Heap for thread commands */
  obstack_init (cproc_obstack);

  add_mach_specific_commands ();
}
