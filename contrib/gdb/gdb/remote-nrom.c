/* Remote debugging with the XLNT Designs, Inc (XDI) NetROM.
   Copyright 1990, 1991, 1992, 1995 Free Software Foundation, Inc.
   Contributed by:
	 Roger Moyers 
	 XLNT Designs, Inc.
	 15050 Avenue of Science, Suite 106
	 San Diego, CA  92128
	 (619)487-9320
	 roger@xlnt.com
   Adapted from work done at Cygnus Support in remote-nindy.c,
   later merged in by Stan Shebs at Cygnus.

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
#include "gdbcmd.h"
#include "serial.h"
#include "target.h"

/* Default ports used to talk with the NetROM.  */

#define DEFAULT_NETROM_LOAD_PORT    1236
#define DEFAULT_NETROM_CONTROL_PORT 1237

static void nrom_close PARAMS ((int quitting));

/* New commands.  */

static void nrom_passthru PARAMS ((char *, int));

/* We talk to the NetROM over these sockets.  */

static serial_t load_desc = NULL;
static serial_t ctrl_desc = NULL;

static int load_port = DEFAULT_NETROM_LOAD_PORT;
static int control_port = DEFAULT_NETROM_CONTROL_PORT;

static char nrom_hostname[100];

/* Forward data declaration. */

extern struct target_ops nrom_ops;

/* Scan input from the remote system, until STRING is found.  Print chars that
   don't match.  */

static int
expect (string)
     char *string;
{
  char *p = string;
  int c;

  immediate_quit = 1;

  while (1)
    {
      c = SERIAL_READCHAR (ctrl_desc, 5);

      if (c == *p++)
	{
	  if (*p == '\0')
	    {
	      immediate_quit = 0;

	      return 0;
	    }
	}
      else
	{
	  fputc_unfiltered (c, gdb_stdout);
	  p = string;
	  if (c == *p)
	    p++;
	}
    }
}

static void
nrom_kill ()
{
  nrom_close (0);
}

static serial_t
open_socket (name, port)
     char *name;
     int port;
{
  char sockname[100];
  serial_t desc;

  sprintf (sockname, "%s:%d", name, port);
  desc = SERIAL_OPEN (sockname);
  if (!desc)
    perror_with_name (sockname);

  return desc;
}

static void
load_cleanup ()
{
  SERIAL_CLOSE (load_desc);
  load_desc = NULL;
}

/* Download a file specified in ARGS to the netROM.  */

static void
nrom_load (args, fromtty)
     char *args;
     int fromtty;
{
  int fd, rd_amt, fsize;
  bfd *pbfd;
  asection *section;
  char *downloadstring = "download 0\n";
  struct cleanup *old_chain;

  /* Tell the netrom to get ready to download. */
  if (SERIAL_WRITE (ctrl_desc, downloadstring, strlen (downloadstring)))
    error ("nrom_load: control_send() of `%s' failed", downloadstring);

  expect ("Waiting for a connection...\n");

  load_desc = open_socket (nrom_hostname, load_port);

  old_chain = make_cleanup (load_cleanup, 0);

  pbfd = bfd_openr (args, 0);

  if (pbfd)
    {
      make_cleanup (bfd_close, pbfd);

      if (!bfd_check_format (pbfd, bfd_object)) 
	error ("\"%s\": not in executable format: %s",
	       args, bfd_errmsg (bfd_get_error ()));

      for (section = pbfd->sections; section; section = section->next) 
	{
	  if (bfd_get_section_flags (pbfd, section) & SEC_ALLOC)
	    {
	      bfd_vma section_address;
	      unsigned long section_size;
	      const char *section_name;

	      section_name = bfd_get_section_name (pbfd, section);
	      section_address = bfd_get_section_vma (pbfd, section);
	      section_size = bfd_section_size (pbfd, section);

	      if (bfd_get_section_flags (pbfd, section) & SEC_LOAD)
		{
		  file_ptr fptr;

		  printf_filtered ("[Loading section %s at %x (%d bytes)]\n",
				   section_name, section_address,
				   section_size);

		  fptr = 0;

		  while (section_size > 0)
		    {
		      char buffer[1024];
		      int count;
		      
		      count = min (section_size, 1024);

		      bfd_get_section_contents (pbfd, section, buffer, fptr,
						count);

		      SERIAL_WRITE (load_desc, buffer, count);
		      section_address += count;
		      fptr += count;
		      section_size -= count;
		    }
		}
	      else			/* BSS and such */
		{
		  printf_filtered ("[section %s: not loading]\n",
				   section_name);
		}
	    }
	}
    }
  else
    error ("\"%s\": Could not open", args);

  do_cleanups (old_chain);
}

/* Open a connection to the remote NetROM devices.  */

static void
nrom_open (name, from_tty)
     char *name;
     int from_tty;
{
  int errn;

  if (!name || strchr (name, '/') || strchr (name, ':'))
    error (
"To open a NetROM connection, you must specify the hostname\n\
or IP address of the NetROM device you wish to use.");

  strcpy (nrom_hostname, name);

  target_preopen (from_tty);

  unpush_target (&nrom_ops);

  ctrl_desc = open_socket (nrom_hostname, control_port);

  push_target (&nrom_ops);

  if (from_tty)
    printf_filtered ("Connected to NetROM device \"%s\"\n", nrom_hostname);
}

/* Close out all files and local state before this target loses control. */

static void
nrom_close (quitting)
     int quitting;
{
  if (load_desc)
    SERIAL_CLOSE (load_desc);
  if (ctrl_desc)
    SERIAL_CLOSE (ctrl_desc);
}

/* Pass arguments directly to the NetROM. */

static void
nrom_passthru (args, fromtty)
     char *args;
     int fromtty;
{
  char buf[1024];

  sprintf (buf, "%s\n", args);
  if (SERIAL_WRITE (ctrl_desc, buf, strlen (buf)))
    error ("nrom_reset: control_send() of `%s'failed", args);
}

static void
nrom_mourn() 
{ 
  unpush_target (&nrom_ops);
  generic_mourn_inferior ();
}

/* Define the target vector. */

struct target_ops nrom_ops = {
  "nrom",			/* to_shortname */
  "Remote XDI `NetROM' target",	/* to_longname */
  "Remote debug using a NetROM over Ethernet",  /* to_doc */
  nrom_open,			/* to_open */
  nrom_close,			/* to_close */
  NULL,				/* to_attach */
  NULL,				/* to_detach */
  NULL,				/* to_resume */
  NULL,				/* to_wait */
  NULL,				/* to_fetch_registers */
  NULL,				/* to_store_registers */
  NULL,				/* to_prepare_to_store */
  NULL,				/* to_xfer_memory */
  NULL,				/* to_files_info */
  NULL,				/* to_insert_breakpoint */
  NULL,				/* to_remove_breakpoint */
  NULL,				/* to_terminal_init */
  NULL,				/* to_terminal_inferior */
  NULL,				/* to_terminal_ours_for_output */
  NULL,				/* to_terminal_ours */
  NULL,				/* to_terminal_info */
  nrom_kill,			/* to_kill */
  nrom_load,			/* to_load */
  NULL,				/* to_lookup_symbol */
  NULL,				/* to_create_inferior */
  nrom_mourn,			/* to_mourn_inferior */
  NULL,				/* to_can_run */
  0,				/* to_notice_signals */
  0,				/* to_thread_alive */
  0,				/* to_stop */
  download_stratum,		/* to_stratum */
  NULL,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  0,				/* to_has_execution */
  NULL,				/* sections */
  NULL,				/* sections_end */
  OPS_MAGIC			/* to_magic */
};

void
_initialize_remote_nrom ()
{
  add_target (&nrom_ops);

  add_show_from_set (
    add_set_cmd ("nrom_load_port", no_class, var_zinteger, (char *)&load_port,
		 "Set the port to use for NetROM downloads\n", &setlist),
		     &showlist);

  add_show_from_set (
    add_set_cmd ("nrom_control_port", no_class, var_zinteger, (char *)&control_port,
		 "Set the port to use for NetROM debugger services\n", &setlist),
		     &showlist);

  add_cmd ("nrom", no_class, nrom_passthru,
	   "Pass arguments as command to NetROM",
	   &cmdlist);
}
