/* GNU/Linux specific methods for using the /proc file system.

   Copyright 2001, 2002 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "inferior.h"
#include <sys/param.h>	/* for MAXPATHLEN */
#include <sys/procfs.h>	/* for elf_gregset etc. */
#include <sys/stat.h>	/* for struct stat */
#include <ctype.h>	/* for isdigit */
#include "regcache.h"	/* for registers_changed */
#include "gregset.h"	/* for gregset */
#include "gdbcore.h"	/* for get_exec_file */
#include "gdbthread.h"	/* for struct thread_info etc. */
#include "elf-bfd.h"	/* for elfcore_write_* */
#include "cli/cli-decode.h"	/* for add_info */

/* Function: child_pid_to_exec_file
 *
 * Accepts an integer pid
 * Returns a string representing a file that can be opened
 * to get the symbols for the child process.
 */

char *
child_pid_to_exec_file (int pid)
{
  char *name1, *name2;

  name1 = xmalloc (MAXPATHLEN);
  name2 = xmalloc (MAXPATHLEN);
  make_cleanup (xfree, name1);
  make_cleanup (xfree, name2);
  memset (name2, 0, MAXPATHLEN);

  sprintf (name1, "/proc/%d/exe", pid);
  if (readlink (name1, name2, MAXPATHLEN) > 0)
    return name2;
  else
    return name1;
}

/* Function: read_mappings
 *
 * Service function for corefiles and info proc.
 */

static int 
read_mapping (FILE *mapfile, 
	      long long *addr, 
	      long long *endaddr, 
	      char *permissions, 
	      long long *offset, 
	      char *device, 
	      long long *inode, 
	      char *filename)
{
  int ret = fscanf (mapfile,  "%llx-%llx %s %llx %s %llx", 
		    addr, endaddr, permissions, offset, device, inode);

  if (ret > 0 && ret != EOF && *inode != 0)
    {
      ret += fscanf (mapfile, "%s\n", filename);
    }
  else
    {
      filename[0] = '\0';	/* no filename */
      fscanf (mapfile, "\n");
    }
  return (ret != 0 && ret != EOF);
}

/* Function: linux_find_memory_regions
 *
 * Fills the "to_find_memory_regions" target vector.
 * Lists the memory regions in the inferior for a corefile.
 */

static int
linux_find_memory_regions (int (*func) (CORE_ADDR, 
					unsigned long,
					int, int, int,
					void *), 
			   void *obfd)
{
  long long pid = PIDGET (inferior_ptid);
  char mapsfilename[MAXPATHLEN];
  FILE *mapsfile;
  long long addr, endaddr, size, offset, inode;
  char permissions[8], device[8], filename[MAXPATHLEN];
  int read, write, exec;
  int ret;

  /* Compose the filename for the /proc memory map, and open it. */
  sprintf (mapsfilename, "/proc/%lld/maps", pid);
  if ((mapsfile = fopen (mapsfilename, "r")) == NULL)
    error ("Could not open %s\n", mapsfilename);

  if (info_verbose)
    fprintf_filtered (gdb_stdout, 
		      "Reading memory regions from %s\n", mapsfilename);

  /* Now iterate until end-of-file. */
  while (read_mapping (mapsfile, &addr, &endaddr, &permissions[0], 
		       &offset, &device[0], &inode, &filename[0]))
    {
      size = endaddr - addr;

      /* Get the segment's permissions.  */
      read  = (strchr (permissions, 'r') != 0);
      write = (strchr (permissions, 'w') != 0);
      exec  = (strchr (permissions, 'x') != 0);

      if (info_verbose)
	{
	  fprintf_filtered (gdb_stdout, 
			    "Save segment, %lld bytes at 0x%s (%c%c%c)", 
			    size, paddr_nz (addr), 
			    read  ? 'r' : ' ', 
			    write ? 'w' : ' ',
			    exec  ? 'x' : ' ');
	  if (filename && filename[0])
	    fprintf_filtered (gdb_stdout, 
			      " for %s", filename);
	  fprintf_filtered (gdb_stdout, "\n");
	}

      /* Invoke the callback function to create the corefile segment. */
      func (addr, size, read, write, exec, obfd);
    }
  fclose (mapsfile);
  return 0;
}

/* Function: linux_do_thread_registers
 *
 * Records the thread's register state for the corefile note section.
 */

static char *
linux_do_thread_registers (bfd *obfd, ptid_t ptid, 
			   char *note_data, int *note_size)
{
  gdb_gregset_t gregs;
  gdb_fpregset_t fpregs;
  unsigned long merged_pid = ptid_get_tid (ptid) << 16 | ptid_get_pid (ptid);

  fill_gregset (&gregs, -1);
  note_data = (char *) elfcore_write_prstatus (obfd, 
					       note_data, 
					       note_size, 
					       merged_pid, 
					       stop_signal, 
					       &gregs);

  fill_fpregset (&fpregs, -1);
  note_data = (char *) elfcore_write_prfpreg (obfd, 
					      note_data, 
					      note_size, 
					      &fpregs, 
					      sizeof (fpregs));
  return note_data;
}

struct linux_corefile_thread_data {
  bfd  *obfd;
  char *note_data;
  int  *note_size;
};

/* Function: linux_corefile_thread_callback
 * 
 * Called by gdbthread.c once per thread.
 * Records the thread's register state for the corefile note section.
 */

static int
linux_corefile_thread_callback (struct thread_info *ti, void *data)
{
  struct linux_corefile_thread_data *args = data;
  ptid_t saved_ptid = inferior_ptid;

  inferior_ptid = ti->ptid;
  registers_changed ();
  target_fetch_registers (-1);	/* FIXME should not be necessary; 
				   fill_gregset should do it automatically. */
  args->note_data = linux_do_thread_registers (args->obfd, 
					       ti->ptid, 
					       args->note_data, 
					       args->note_size);
  inferior_ptid = saved_ptid;
  registers_changed ();
  target_fetch_registers (-1);	/* FIXME should not be necessary; 
				   fill_gregset should do it automatically. */
  return 0;
}

/* Function: linux_make_note_section
 *
 * Fills the "to_make_corefile_note" target vector.
 * Builds the note section for a corefile, and returns it 
 * in a malloc buffer. 
 */

static char *
linux_make_note_section (bfd *obfd, int *note_size)
{
  struct linux_corefile_thread_data thread_args;
  struct cleanup *old_chain;
  char fname[16] = {'\0'};
  char psargs[80] = {'\0'};
  char *note_data = NULL;
  ptid_t current_ptid = inferior_ptid;

  if (get_exec_file (0))
    {
      strncpy (fname, strrchr (get_exec_file (0), '/') + 1, sizeof (fname));
      strncpy (psargs, get_exec_file (0), 
	       sizeof (psargs));
      if (get_inferior_args ())
	{
	  strncat (psargs, " ", 
		   sizeof (psargs) - strlen (psargs));
	  strncat (psargs, get_inferior_args (), 
		   sizeof (psargs) - strlen (psargs));
	}
      note_data = (char *) elfcore_write_prpsinfo (obfd, 
						   note_data, 
						   note_size, 
						   fname, 
						   psargs);
    }

  /* Dump information for threads.  */
  thread_args.obfd = obfd;
  thread_args.note_data = note_data;
  thread_args.note_size = note_size;
  iterate_over_threads (linux_corefile_thread_callback, &thread_args);
  if (thread_args.note_data == note_data)
    {
      /* iterate_over_threads didn't come up with any threads;
	 just use inferior_ptid. */
      note_data = linux_do_thread_registers (obfd, inferior_ptid, 
					     note_data, note_size);
    }
  else
    {
      note_data = thread_args.note_data;
    }

  make_cleanup (xfree, note_data);
  return note_data;
}

/*
 * Function: linux_info_proc_cmd
 *
 * Implement the "info proc" command.
 */

static void
linux_info_proc_cmd (char *args, int from_tty)
{
  long long pid = PIDGET (inferior_ptid);
  FILE *procfile;
  char **argv = NULL;
  char buffer[MAXPATHLEN];
  char fname1[MAXPATHLEN], fname2[MAXPATHLEN];
  int cmdline_f = 1;
  int cwd_f = 1;
  int exe_f = 1;
  int mappings_f = 0;
  int environ_f = 0;
  int status_f = 0;
  int stat_f = 0;
  int all = 0;
  struct stat dummy;

  if (args)
    {
      /* Break up 'args' into an argv array. */
      if ((argv = buildargv (args)) == NULL)
	nomem (0);
      else
	make_cleanup_freeargv (argv);
    }
  while (argv != NULL && *argv != NULL)
    {
      if (isdigit (argv[0][0]))
	{
	  pid = strtoul (argv[0], NULL, 10);
	}
      else if (strncmp (argv[0], "mappings", strlen (argv[0])) == 0)
	{
	  mappings_f = 1;
	}
      else if (strcmp (argv[0], "status") == 0)
	{
	  status_f = 1;
	}
      else if (strcmp (argv[0], "stat") == 0)
	{
	  stat_f = 1;
	}
      else if (strcmp (argv[0], "cmd") == 0)
	{
	  cmdline_f = 1;
	}
      else if (strncmp (argv[0], "exe", strlen (argv[0])) == 0)
	{
	  exe_f = 1;
	}
      else if (strcmp (argv[0], "cwd") == 0)
	{
	  cwd_f = 1;
	}
      else if (strncmp (argv[0], "all", strlen (argv[0])) == 0)
	{
	  all = 1;
	}
      else
	{
	  /* [...] (future options here) */
	}
      argv++;
    }
  if (pid == 0)
    error ("No current process: you must name one.");

  sprintf (fname1, "/proc/%lld", pid);
  if (stat (fname1, &dummy) != 0)
    error ("No /proc directory: '%s'", fname1);

  printf_filtered ("process %lld\n", pid);
  if (cmdline_f || all)
    {
      sprintf (fname1, "/proc/%lld/cmdline", pid);
      if ((procfile = fopen (fname1, "r")) > 0)
	{
	  fgets (buffer, sizeof (buffer), procfile);
	  printf_filtered ("cmdline = '%s'\n", buffer);
	  fclose (procfile);
	}
      else
	warning ("unable to open /proc file '%s'", fname1);
    }
  if (cwd_f || all)
    {
      sprintf (fname1, "/proc/%lld/cwd", pid);
      memset (fname2, 0, sizeof (fname2));
      if (readlink (fname1, fname2, sizeof (fname2)) > 0)
	printf_filtered ("cwd = '%s'\n", fname2);
      else
	warning ("unable to read link '%s'", fname1);
    }
  if (exe_f || all)
    {
      sprintf (fname1, "/proc/%lld/exe", pid);
      memset (fname2, 0, sizeof (fname2));
      if (readlink (fname1, fname2, sizeof (fname2)) > 0)
	printf_filtered ("exe = '%s'\n", fname2);
      else
	warning ("unable to read link '%s'", fname1);
    }
  if (mappings_f || all)
    {
      sprintf (fname1, "/proc/%lld/maps", pid);
      if ((procfile = fopen (fname1, "r")) > 0)
	{
	  long long addr, endaddr, size, offset, inode;
	  char permissions[8], device[8], filename[MAXPATHLEN];
	  char *header_fmt_string, *data_fmt_string;

	  if (TARGET_ADDR_BIT == 32)
	    {
	      header_fmt_string = "\t%10s %10s %10s %10s %7s\n";
	      data_fmt_string   = "\t%#10lx %#10lx %#10x %#10x %7s\n";
	    }
	  else
	    {
	      header_fmt_string = "  %18s %18s %10s %10s %7s\n";
	      data_fmt_string   = "  %#18lx %#18lx %#10x %#10x %7s\n";
	    }

	  printf_filtered ("Mapped address spaces:\n\n");
	  printf_filtered (header_fmt_string, 
			   "Start Addr",
			   "  End Addr",
			   "      Size",
			   "    Offset",
			   "objfile");
	  
	  while (read_mapping (procfile, &addr, &endaddr, &permissions[0], 
			       &offset, &device[0], &inode, &filename[0]))
	    {
	      size = endaddr - addr;
	      printf_filtered (data_fmt_string, 
			       (unsigned long) addr, /* FIXME: pr_addr */
			       (unsigned long) endaddr, 
			       (int) size, 
			       (unsigned int) offset, 
			       filename[0] ? filename : "");
	      
	    }

	  fclose (procfile);
	}
      else
	warning ("unable to open /proc file '%s'", fname1);
    }
  if (status_f || all)
    {
      sprintf (fname1, "/proc/%lld/status", pid);
      if ((procfile = fopen (fname1, "r")) > 0)
	{
	  while (fgets (buffer, sizeof (buffer), procfile) != NULL)
	    printf_filtered (buffer);
	  fclose (procfile);
	}
      else 
	warning ("unable to open /proc file '%s'", fname1);
    }
  if (stat_f || all)
    {
      sprintf (fname1, "/proc/%lld/stat", pid);
      if ((procfile = fopen (fname1, "r")) > 0)
	{
	  int itmp;
	  char ctmp;

	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Process: %d\n", itmp);
	  if (fscanf (procfile, "%s ", &buffer[0]) > 0)
	    printf_filtered ("Exec file: %s\n", buffer);
	  if (fscanf (procfile, "%c ", &ctmp) > 0)
	    printf_filtered ("State: %c\n", ctmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Parent process: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Process group: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Session id: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("TTY: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("TTY owner process group: %d\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Flags: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Minor faults (no memory page): %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Minor faults, children: %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Major faults (memory page faults): %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Major faults, children: %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("utime: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("stime: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("utime, children: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("stime, children: %d\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("jiffies remaining in current time slice: %d\n", 
			     itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("'nice' value: %d\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("jiffies until next timeout: %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("jiffies until next SIGALRM: %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("start time (jiffies since system boot): %d\n", 
			     itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Virtual memory size: %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Resident set size: %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("rlim: %u\n", 
			     (unsigned int) itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Start of text: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("End of text: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)
	    printf_filtered ("Start of stack: 0x%x\n", itmp);
#if 0	/* Don't know how architecture-dependent the rest is... 
	   Anyway the signal bitmap info is available from "status".  */
	  if (fscanf (procfile, "%u ", &itmp) > 0)	/* FIXME arch? */
	    printf_filtered ("Kernel stack pointer: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)	/* FIXME arch? */
	    printf_filtered ("Kernel instr pointer: 0x%x\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Pending signals bitmap: 0x%x\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Blocked signals bitmap: 0x%x\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Ignored signals bitmap: 0x%x\n", itmp);
	  if (fscanf (procfile, "%d ", &itmp) > 0)
	    printf_filtered ("Catched signals bitmap: 0x%x\n", itmp);
	  if (fscanf (procfile, "%u ", &itmp) > 0)	/* FIXME arch? */
	    printf_filtered ("wchan (system call): 0x%x\n", itmp);
#endif
	  fclose (procfile);
	}
      else
	warning ("unable to open /proc file '%s'", fname1);
    }
}

void
_initialize_linux_proc (void)
{
  extern void inftarg_set_find_memory_regions ();
  extern void inftarg_set_make_corefile_notes ();

  inftarg_set_find_memory_regions (linux_find_memory_regions);
  inftarg_set_make_corefile_notes (linux_make_note_section);

  add_info ("proc", linux_info_proc_cmd, 
	    "Show /proc process information about any running process.\n\
Specify any process id, or use the program being debugged by default.\n\
Specify any of the following keywords for detailed info:\n\
  mappings -- list of mapped memory regions.\n\
  stat     -- list a bunch of random process info.\n\
  status   -- list a different bunch of random process info.\n\
  all      -- list all available /proc info.");
}
