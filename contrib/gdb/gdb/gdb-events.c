/* User Interface Events.
   Copyright 1999, 2001 Free Software Foundation, Inc.

   Contributed by Cygnus Solutions.

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

/* Work in progress */

/* This file was created with the aid of ``gdb-events.sh''.

   The bourn shell script ``gdb-events.sh'' creates the files
   ``new-gdb-events.c'' and ``new-gdb-events.h and then compares
   them against the existing ``gdb-events.[hc]''.  Any differences
   found being reported.

   If editing this file, please also run gdb-events.sh and merge any
   changes into that script. Conversely, when making sweeping changes
   to this file, modifying gdb-events.sh and using its output may
   prove easier. */


#include "defs.h"
#include "gdb-events.h"
#include "gdbcmd.h"

#undef XMALLOC
#define XMALLOC(TYPE) ((TYPE*) xmalloc (sizeof (TYPE)))

#if WITH_GDB_EVENTS
static struct gdb_events null_event_hooks;
static struct gdb_events queue_event_hooks;
static struct gdb_events *current_event_hooks = &null_event_hooks;
#endif

int gdb_events_debug;

#if WITH_GDB_EVENTS

void
breakpoint_create_event (int b)
{
  if (gdb_events_debug)
    fprintf_unfiltered (gdb_stdlog, "breakpoint_create_event\n");
  if (!current_event_hooks->breakpoint_create)
    return;
  current_event_hooks->breakpoint_create (b);
}

void
breakpoint_delete_event (int b)
{
  if (gdb_events_debug)
    fprintf_unfiltered (gdb_stdlog, "breakpoint_delete_event\n");
  if (!current_event_hooks->breakpoint_delete)
    return;
  current_event_hooks->breakpoint_delete (b);
}

void
breakpoint_modify_event (int b)
{
  if (gdb_events_debug)
    fprintf_unfiltered (gdb_stdlog, "breakpoint_modify_event\n");
  if (!current_event_hooks->breakpoint_modify)
    return;
  current_event_hooks->breakpoint_modify (b);
}

void
tracepoint_create_event (int number)
{
  if (gdb_events_debug)
    fprintf_unfiltered (gdb_stdlog, "tracepoint_create_event\n");
  if (!current_event_hooks->tracepoint_create)
    return;
  current_event_hooks->tracepoint_create (number);
}

void
tracepoint_delete_event (int number)
{
  if (gdb_events_debug)
    fprintf_unfiltered (gdb_stdlog, "tracepoint_delete_event\n");
  if (!current_event_hooks->tracepoint_delete)
    return;
  current_event_hooks->tracepoint_delete (number);
}

void
tracepoint_modify_event (int number)
{
  if (gdb_events_debug)
    fprintf_unfiltered (gdb_stdlog, "tracepoint_modify_event\n");
  if (!current_event_hooks->tracepoint_modify)
    return;
  current_event_hooks->tracepoint_modify (number);
}

void
architecture_changed_event (void)
{
  if (gdb_events_debug)
    fprintf_unfiltered (gdb_stdlog, "architecture_changed_event\n");
  if (!current_event_hooks->architecture_changed)
    return;
  current_event_hooks->architecture_changed ();
}

#endif

#if WITH_GDB_EVENTS
struct gdb_events *
set_gdb_event_hooks (struct gdb_events *vector)
{
  struct gdb_events *old_events = current_event_hooks;
  if (vector == NULL)
    current_event_hooks = &queue_event_hooks;
  else
    current_event_hooks = vector;
  return old_events;
}
#endif

enum gdb_event
{
  breakpoint_create,
  breakpoint_delete,
  breakpoint_modify,
  tracepoint_create,
  tracepoint_delete,
  tracepoint_modify,
  architecture_changed,
  nr_gdb_events
};

struct breakpoint_create
  {
    int b;
  };

struct breakpoint_delete
  {
    int b;
  };

struct breakpoint_modify
  {
    int b;
  };

struct tracepoint_create
  {
    int number;
  };

struct tracepoint_delete
  {
    int number;
  };

struct tracepoint_modify
  {
    int number;
  };

struct event
  {
    enum gdb_event type;
    struct event *next;
    union
      {
	struct breakpoint_create breakpoint_create;
	struct breakpoint_delete breakpoint_delete;
	struct breakpoint_modify breakpoint_modify;
	struct tracepoint_create tracepoint_create;
	struct tracepoint_delete tracepoint_delete;
	struct tracepoint_modify tracepoint_modify;
      }
    data;
  };
struct event *pending_events;
struct event *delivering_events;

static void
append (struct event *new_event)
{
  struct event **event = &pending_events;
  while ((*event) != NULL)
    event = &((*event)->next);
  (*event) = new_event;
  (*event)->next = NULL;
}

static void
queue_breakpoint_create (int b)
{
  struct event *event = XMALLOC (struct event);
  event->type = breakpoint_create;
  event->data.breakpoint_create.b = b;
  append (event);
}

static void
queue_breakpoint_delete (int b)
{
  struct event *event = XMALLOC (struct event);
  event->type = breakpoint_delete;
  event->data.breakpoint_delete.b = b;
  append (event);
}

static void
queue_breakpoint_modify (int b)
{
  struct event *event = XMALLOC (struct event);
  event->type = breakpoint_modify;
  event->data.breakpoint_modify.b = b;
  append (event);
}

static void
queue_tracepoint_create (int number)
{
  struct event *event = XMALLOC (struct event);
  event->type = tracepoint_create;
  event->data.tracepoint_create.number = number;
  append (event);
}

static void
queue_tracepoint_delete (int number)
{
  struct event *event = XMALLOC (struct event);
  event->type = tracepoint_delete;
  event->data.tracepoint_delete.number = number;
  append (event);
}

static void
queue_tracepoint_modify (int number)
{
  struct event *event = XMALLOC (struct event);
  event->type = tracepoint_modify;
  event->data.tracepoint_modify.number = number;
  append (event);
}

static void
queue_architecture_changed (void)
{
  struct event *event = XMALLOC (struct event);
  event->type = architecture_changed;
  append (event);
}

void
gdb_events_deliver (struct gdb_events *vector)
{
  /* Just zap any events left around from last time. */
  while (delivering_events != NULL)
    {
      struct event *event = delivering_events;
      delivering_events = event->next;
      xfree (event);
    }
  /* Process any pending events.  Because one of the deliveries could
     bail out we move everything off of the pending queue onto an
     in-progress queue where it can, later, be cleaned up if
     necessary. */
  delivering_events = pending_events;
  pending_events = NULL;
  while (delivering_events != NULL)
    {
      struct event *event = delivering_events;
      switch (event->type)
	{
	case breakpoint_create:
	  vector->breakpoint_create
	    (event->data.breakpoint_create.b);
	  break;
	case breakpoint_delete:
	  vector->breakpoint_delete
	    (event->data.breakpoint_delete.b);
	  break;
	case breakpoint_modify:
	  vector->breakpoint_modify
	    (event->data.breakpoint_modify.b);
	  break;
	case tracepoint_create:
	  vector->tracepoint_create
	    (event->data.tracepoint_create.number);
	  break;
	case tracepoint_delete:
	  vector->tracepoint_delete
	    (event->data.tracepoint_delete.number);
	  break;
	case tracepoint_modify:
	  vector->tracepoint_modify
	    (event->data.tracepoint_modify.number);
	  break;
	case architecture_changed:
	  vector->architecture_changed ();
	  break;
	}
      delivering_events = event->next;
      xfree (event);
    }
}

void _initialize_gdb_events (void);
void
_initialize_gdb_events (void)
{
  struct cmd_list_element *c;
#if WITH_GDB_EVENTS
  queue_event_hooks.breakpoint_create = queue_breakpoint_create;
  queue_event_hooks.breakpoint_delete = queue_breakpoint_delete;
  queue_event_hooks.breakpoint_modify = queue_breakpoint_modify;
  queue_event_hooks.tracepoint_create = queue_tracepoint_create;
  queue_event_hooks.tracepoint_delete = queue_tracepoint_delete;
  queue_event_hooks.tracepoint_modify = queue_tracepoint_modify;
  queue_event_hooks.architecture_changed = queue_architecture_changed;
#endif

  c = add_set_cmd ("eventdebug", class_maintenance, var_zinteger,
		   (char *) (&gdb_events_debug), "Set event debugging.\n\
When non-zero, event/notify debugging is enabled.", &setlist);
  deprecate_cmd (c, "set debug event");
  deprecate_cmd (add_show_from_set (c, &showlist), "show debug event");

  add_show_from_set (add_set_cmd ("event",
				  class_maintenance,
				  var_zinteger,
				  (char *) (&gdb_events_debug),
				  "Set event debugging.\n\
When non-zero, event/notify debugging is enabled.", &setdebuglist),
		     &showdebuglist);
}
