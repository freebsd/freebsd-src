/* -*- Mode:C; tab-width: 8 -*-
 * remote.c --- remote control of Netscape Navigator for Unix.
 * version 1.1.3, for Netscape Navigator 1.1 and newer.
 *
 * Copyright © 1996 Netscape Communications Corporation, all rights reserved.
 * Created: Jamie Zawinski <jwz@netscape.com>, 24-Dec-94.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * To compile:
 *
 *    cc -o netscape-remote remote.c -DSTANDALONE -lXmu -lX11
 *
 * To use:
 *
 *    netscape-remote -help
 *
 * Documentation for the protocol which this code implements may be found at:
 *
 *    http://home.netscape.com/newsref/std/x-remote.html
 *
 * Bugs and commentary to x_cbug@netscape.com.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/WinUtil.h>	/* for XmuClientWindow() */


/* vroot.h is a header file which lets a client get along with `virtual root'
   window managers like swm, tvtwm, olvwm, etc.  If you don't have this header
   file, you can find it at "http://home.netscape.com/newsref/std/vroot.h".
   If you don't care about supporting virtual root window managers, you can
   comment this line out.
 */
#include "vroot.h"


#ifdef STANDALONE
#ifdef GLOBAL
 extern char *progname;
#else
 static const char *progname = 0;
#endif
 static const char *expected_mozilla_version = "1.1";
#else  /* !STANDALONE */
 extern const char *progname;
 extern const char *expected_mozilla_version;
#endif /* !STANDALONE */

#define MOZILLA_VERSION_PROP   "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP      "_MOZILLA_LOCK"
#define MOZILLA_COMMAND_PROP   "_MOZILLA_COMMAND"
#define MOZILLA_RESPONSE_PROP  "_MOZILLA_RESPONSE"
static Atom XA_MOZILLA_VERSION  = 0;
static Atom XA_MOZILLA_LOCK     = 0;
static Atom XA_MOZILLA_COMMAND  = 0;
static Atom XA_MOZILLA_RESPONSE = 0;

static void
mozilla_remote_init_atoms (Display *dpy)
{
  if (! XA_MOZILLA_VERSION)
    XA_MOZILLA_VERSION = XInternAtom (dpy, MOZILLA_VERSION_PROP, False);
  if (! XA_MOZILLA_LOCK)
    XA_MOZILLA_LOCK = XInternAtom (dpy, MOZILLA_LOCK_PROP, False);
  if (! XA_MOZILLA_COMMAND)
    XA_MOZILLA_COMMAND = XInternAtom (dpy, MOZILLA_COMMAND_PROP, False);
  if (! XA_MOZILLA_RESPONSE)
    XA_MOZILLA_RESPONSE = XInternAtom (dpy, MOZILLA_RESPONSE_PROP, False);
}

static Window
mozilla_remote_find_window (Display *dpy)
{
  int i;
  Window root = RootWindowOfScreen (DefaultScreenOfDisplay (dpy));
  Window root2, parent, *kids;
  unsigned int nkids;
  Window result = 0;
  Window tenative = 0;
  unsigned char *tenative_version = 0;

  if (! XQueryTree (dpy, root, &root2, &parent, &kids, &nkids))
    {
      fprintf (stderr, "%s: XQueryTree failed on display %s\n", progname,
	       DisplayString (dpy));
      exit (2);
    }

  /* root != root2 is possible with virtual root WMs. */

  if (! (kids && nkids))
    {
      fprintf (stderr, "%s: root window has no children on display %s\n",
	       progname, DisplayString (dpy));
      exit (2);
    }

  for (i = nkids-1; i >= 0; i--)
    {
      Atom type;
      int format;
      unsigned long nitems, bytesafter;
      unsigned char *version = 0;
      Window w = XmuClientWindow (dpy, kids[i]);
      int status = XGetWindowProperty (dpy, w, XA_MOZILLA_VERSION,
				       0, (65536 / sizeof (long)),
				       False, XA_STRING,
				       &type, &format, &nitems, &bytesafter,
				       &version);
      if (! version)
	continue;
      if (strcmp ((char *) version, expected_mozilla_version) &&
	  !tenative)
	{
	  tenative = w;
	  tenative_version = version;
	  continue;
	}
      XFree (version);
      if (status == Success && type != None)
	{
	  result = w;
	  break;
	}
    }

  if (result && tenative)
    {
#ifndef GLOBAL
      fprintf (stderr,
	       "%s: warning: both version %s (0x%x) and version\n"
	       "\t%s (0x%x) are running.  Using version %s.\n",
	       progname, tenative_version, (unsigned int) tenative,
	       expected_mozilla_version, (unsigned int) result,
	       expected_mozilla_version);
#endif
      XFree (tenative_version);
      return result;
    }
  else if (tenative)
    {
#ifndef GLOBAL
      fprintf (stderr,
	       "%s: warning: expected version %s but found version\n"
	       "\t%s (0x%x) instead.\n",
	       progname, expected_mozilla_version,
	       tenative_version, (unsigned int) tenative);
#endif
      XFree (tenative_version);
      return tenative;
    }
  else if (result)
    {
      return result;
    }
  else
    {
#ifdef GLOBAL
      return 0;
#else
      fprintf (stderr, "%s: not running on display %s\n", progname,
	       DisplayString (dpy));
      exit (1);
#endif
    }
}

static void
mozilla_remote_check_window (Display *dpy, Window window)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *version = 0;
  int status = XGetWindowProperty (dpy, window, XA_MOZILLA_VERSION,
				   0, (65536 / sizeof (long)),
				   False, XA_STRING,
				   &type, &format, &nitems, &bytesafter,
				   &version);
  if (status != Success || !version)
    {
      fprintf (stderr, "%s: window 0x%x is not a Netscape window.\n",
	       progname, (unsigned int) window);
      exit (6);
    }
  else if (strcmp ((char *) version, expected_mozilla_version))
    {
      fprintf (stderr,
	       "%s: warning: window 0x%x is Netscape version %s;\n"
	       "\texpected version %s.\n",
	       progname, (unsigned int) window,
	       version, expected_mozilla_version);
    }
  XFree (version);
}


static char *lock_data = 0;

static void
mozilla_remote_obtain_lock (Display *dpy, Window window)
{
  Bool locked = False;
  Bool waited = False;

  if (! lock_data)
    {
      lock_data = (char *) malloc (255);
      sprintf (lock_data, "pid%d@", getpid ());
      if (gethostname (lock_data + strlen (lock_data), 100))
	{
	  perror ("gethostname");
	  exit (-1);
	}
    }

  do
    {
      int result;
      Atom actual_type;
      int actual_format;
      unsigned long nitems, bytes_after;
      unsigned char *data = 0;

      XGrabServer (dpy);   /* ################################# DANGER! */

      result = XGetWindowProperty (dpy, window, XA_MOZILLA_LOCK,
				   0, (65536 / sizeof (long)),
				   False, /* don't delete */
				   XA_STRING,
				   &actual_type, &actual_format,
				   &nitems, &bytes_after,
				   &data);
      if (result != Success || actual_type == None)
	{
	  /* It's not now locked - lock it. */
#ifdef DEBUG_PROPS
	  fprintf (stderr, "%s: (writing " MOZILLA_LOCK_PROP
		   " \"%s\" to 0x%x)\n",
		   progname, lock_data, (unsigned int) window);
#endif
	  XChangeProperty (dpy, window, XA_MOZILLA_LOCK, XA_STRING, 8,
			   PropModeReplace, (unsigned char *) lock_data,
			   strlen (lock_data));
	  locked = True;
	}

      XUngrabServer (dpy); /* ################################# danger over */
      XSync (dpy, False);

      if (! locked)
	{
	  /* We tried to grab the lock this time, and failed because someone
	     else is holding it already.  So, wait for a PropertyDelete event
	     to come in, and try again. */

	  fprintf (stderr, "%s: window 0x%x is locked by %s; waiting...\n",
		   progname, (unsigned int) window, data);
	  waited = True;

	  while (1)
	    {
	      XEvent event;
	      XNextEvent (dpy, &event);
	      if (event.xany.type == DestroyNotify &&
		  event.xdestroywindow.window == window)
		{
		  fprintf (stderr, "%s: window 0x%x unexpectedly destroyed.\n",
			   progname, (unsigned int) window);
		  exit (6);
		}
	      else if (event.xany.type == PropertyNotify &&
		       event.xproperty.state == PropertyDelete &&
		       event.xproperty.window == window &&
		       event.xproperty.atom == XA_MOZILLA_LOCK)
		{
		  /* Ok!  Someone deleted their lock, so now we can try
		     again. */
#ifdef DEBUG_PROPS
		  fprintf (stderr, "%s: (0x%x unlocked, trying again...)\n",
			   progname, (unsigned int) window);
#endif
		  break;
		}
	    }
	}
      if (data)
	XFree (data);
    }
  while (! locked);

  if (waited)
    fprintf (stderr, "%s: obtained lock.\n", progname);
}


static void
mozilla_remote_free_lock (Display *dpy, Window window)
{
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *data = 0;

#ifdef DEBUG_PROPS
	  fprintf (stderr, "%s: (deleting " MOZILLA_LOCK_PROP
		   " \"%s\" from 0x%x)\n",
		   progname, lock_data, (unsigned int) window);
#endif

  result = XGetWindowProperty (dpy, window, XA_MOZILLA_LOCK,
			       0, (65536 / sizeof (long)),
			       True, /* atomic delete after */
			       XA_STRING,
			       &actual_type, &actual_format,
			       &nitems, &bytes_after,
			       &data);
  if (result != Success)
    {
      fprintf (stderr, "%s: unable to read and delete " MOZILLA_LOCK_PROP
	       " property\n",
	       progname);
      return;
    }
  else if (!data || !*data)
    {
      fprintf (stderr, "%s: invalid data on " MOZILLA_LOCK_PROP
	       " of window 0x%x.\n",
	       progname, (unsigned int) window);
      return;
    }
  else if (strcmp ((char *) data, lock_data))
    {
      fprintf (stderr, "%s: " MOZILLA_LOCK_PROP
	       " was stolen!  Expected \"%s\", saw \"%s\"!\n",
	       progname, lock_data, data);
      return;
    }

  if (data)
    XFree (data);
}


static int
mozilla_remote_command (Display *dpy, Window window, const char *command,
			Bool raise_p)
{
  int result;
  Bool done = False;
  char *new_command = 0;

  /* The -noraise option is implemented by passing a "noraise" argument
     to each command to which it should apply.
   */
  if (! raise_p)
    {
      char *close;
      new_command = (char *) malloc (strlen (command) + 20);
      strcpy (new_command, command);
      close = strrchr (new_command, ')');
      if (close)
	strcpy (close, ", noraise)");
      else
	strcat (new_command, "(noraise)");
      command = new_command;
    }

#ifdef DEBUG_PROPS
  fprintf (stderr, "%s: (writing " MOZILLA_COMMAND_PROP " \"%s\" to 0x%x)\n",
	   progname, command, (unsigned int) window);
#endif

  XChangeProperty (dpy, window, XA_MOZILLA_COMMAND, XA_STRING, 8,
		   PropModeReplace, (unsigned char *) command,
		   strlen (command));

  while (!done)
    {
      XEvent event;
      XNextEvent (dpy, &event);
      if (event.xany.type == DestroyNotify &&
	  event.xdestroywindow.window == window)
	{
	  /* Print to warn user...*/
	  fprintf (stderr, "%s: window 0x%x was destroyed.\n",
		   progname, (unsigned int) window);
	  result = 6;
	  goto DONE;
	}
      else if (event.xany.type == PropertyNotify &&
	       event.xproperty.state == PropertyNewValue &&
	       event.xproperty.window == window &&
	       event.xproperty.atom == XA_MOZILLA_RESPONSE)
	{
	  Atom actual_type;
	  int actual_format;
	  unsigned long nitems, bytes_after;
	  unsigned char *data = 0;

	  result = XGetWindowProperty (dpy, window, XA_MOZILLA_RESPONSE,
				       0, (65536 / sizeof (long)),
				       True, /* atomic delete after */
				       XA_STRING,
				       &actual_type, &actual_format,
				       &nitems, &bytes_after,
				       &data);
#ifdef DEBUG_PROPS
	  if (result == Success && data && *data)
	    {
	      fprintf (stderr, "%s: (server sent " MOZILLA_RESPONSE_PROP
		       " \"%s\" to 0x%x.)\n",
		       progname, data, (unsigned int) window);
	    }
#endif

	  if (result != Success)
	    {
	      fprintf (stderr, "%s: failed reading " MOZILLA_RESPONSE_PROP
		       " from window 0x%0x.\n",
		       progname, (unsigned int) window);
	      result = 6;
	      done = True;
	    }
	  else if (!data || strlen((char *) data) < 5)
	    {
	      fprintf (stderr, "%s: invalid data on " MOZILLA_RESPONSE_PROP
		       " property of window 0x%0x.\n",
		       progname, (unsigned int) window);
	      result = 6;
	      done = True;
	    }
	  else if (*data == '1')	/* positive preliminary reply */
	    {
	      fprintf (stderr, "%s: %s\n", progname, data + 4);
	      /* keep going */
	      done = False;
	    }
#if 1
	  else if (!strncmp ((char *)data, "200", 3)) /* positive completion */
	    {
	      result = 0;
	      done = True;
	    }
#endif
	  else if (*data == '2')		/* positive completion */
	    {
	      fprintf (stderr, "%s: %s\n", progname, data + 4);
	      result = 0;
	      done = True;
	    }
	  else if (*data == '3')	/* positive intermediate reply */
	    {
	      fprintf (stderr, "%s: internal error: "
		       "server wants more information?  (%s)\n",
		       progname, data);
	      result = 3;
	      done = True;
	    }
	  else if (*data == '4' ||	/* transient negative completion */
		   *data == '5')	/* permanent negative completion */
	    {
	      fprintf (stderr, "%s: %s\n", progname, data + 4);
	      result = (*data - '0');
	      done = True;
	    }
	  else
	    {
	      fprintf (stderr,
		       "%s: unrecognised " MOZILLA_RESPONSE_PROP
		       " from window 0x%x: %s\n",
		       progname, (unsigned int) window, data);
	      result = 6;
	      done = True;
	    }

	  if (data)
	    XFree (data);
	}
#ifdef DEBUG_PROPS
      else if (event.xany.type == PropertyNotify &&
	       event.xproperty.window == window &&
	       event.xproperty.state == PropertyDelete &&
	       event.xproperty.atom == XA_MOZILLA_COMMAND)
	{
	  fprintf (stderr, "%s: (server 0x%x has accepted "
		   MOZILLA_COMMAND_PROP ".)\n",
		   progname, (unsigned int) window);
	}
#endif /* DEBUG_PROPS */
    }

 DONE:

  if (new_command)
    free (new_command);

  return result;
}

int
mozilla_remote_commands (Display *dpy, Window window, char **commands)
{
  Bool raise_p = True;
  int status = 0;
  mozilla_remote_init_atoms (dpy);

  if (window == 0)
    window = mozilla_remote_find_window (dpy);
  else
    mozilla_remote_check_window (dpy, window);
#ifdef GLOBAL
  if (window == 0)
    return -1;
#endif

  XSelectInput (dpy, window, (PropertyChangeMask|StructureNotifyMask));

  mozilla_remote_obtain_lock (dpy, window);

  while (*commands)
    {
      if (!strcmp (*commands, "-raise"))
	raise_p = True;
      else if (!strcmp (*commands, "-noraise"))
	raise_p = False;
      else
	status = mozilla_remote_command (dpy, window, *commands, raise_p);

      if (status != 0)
	break;
      commands++;
    }

  /* When status = 6, it means the window has been destroyed */
  /* It is invalid to free the lock when window is destroyed. */

  if ( status != 6 )
  mozilla_remote_free_lock (dpy, window);

  return status;
}


#ifdef STANDALONE

static void
usage (void)
{
  fprintf (stderr, "usage: %s [ options ... ]\n\
       where options include:\n\
\n\
       -help                     to show this message.\n\
       -display <dpy>            to specify the X server to use.\n\
       -remote <remote-command>  to execute a command in an already-running\n\
                                 Netscape process.  See the manual for a\n\
                                 list of valid commands.\n\
       -id <window-id>           the id of an X window to which the -remote\n\
                                 commands should be sent; if unspecified,\n\
                                 the first window found will be used.\n\
       -raise                    whether following -remote commands should\n\
                                 cause the window to raise itself to the top\n\
                                 (this is the default.)\n\
       -noraise                  the opposite of -raise: following -remote\n\
                                 commands will not auto-raise the window.\n\
",
	   progname);
}


#ifdef GLOBAL
int
netscape_remote(int argc, char **argv)
#else
void
main (int argc, char **argv)
#endif
{
  Display *dpy;
  char *dpy_string = 0;
  char **remote_commands = 0;
  int remote_command_count = 0;
  int remote_command_size = 0;
  unsigned long remote_window = 0;
  Bool sync_p = False;
  int i;

  progname = strrchr (argv[0], '/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  /* Hack the -help and -version arguments before opening the display. */
  for (i = 1; i < argc; i++)
    {
      if (!strcasecmp (argv [i], "-h") ||
	  !strcasecmp (argv [i], "-help"))
	{
	  usage ();
	  exit (0);
	}
      else if (!strcmp (argv [i], "-d") ||
	       !strcmp (argv [i], "-dpy") ||
	       !strcmp (argv [i], "-disp") ||
	       !strcmp (argv [i], "-display"))
	{
	  i++;
	  dpy_string = argv [i];
	}
      else if (!strcmp (argv [i], "-sync") ||
	       !strcmp (argv [i], "-synchronize"))
	{
	  sync_p = True;
	}
      else if (!strcmp (argv [i], "-remote"))
	{
	  if (remote_command_count == remote_command_size)
	    {
	      remote_command_size += 20;
	      remote_commands =
		(remote_commands
		 ? realloc (remote_commands,
			    remote_command_size * sizeof (char *))
		 : calloc (remote_command_size, sizeof (char *)));
	    }
	  i++;
	  if (!argv[i] || *argv[i] == '-' || *argv[i] == 0)
	    {
	      fprintf (stderr, "%s: invalid `-remote' option \"%s\"\n",
		       progname, argv[i] ? argv[i] : "");
	      usage ();
	      exit (-1);
	    }
	  remote_commands [remote_command_count++] = argv[i];
	}
      else if (!strcmp (argv [i], "-raise") ||
	       !strcmp (argv [i], "-noraise"))
	{
	  char *r = argv [i];
	  if (remote_command_count == remote_command_size)
	    {
	      remote_command_size += 20;
	      remote_commands =
		(remote_commands
		 ? realloc (remote_commands,
			    remote_command_size * sizeof (char *))
		 : calloc (remote_command_size, sizeof (char *)));
	    }
	  remote_commands [remote_command_count++] = r;
	}
      else if (!strcmp (argv [i], "-id"))
	{
	  char c;
	  if (remote_command_count > 0)
	    {
	      fprintf (stderr,
		"%s: the `-id' option must preceed all `-remote' options.\n",
		       progname);
	      usage ();
	      exit (-1);
	    }
	  else if (remote_window != 0)
	    {
	      fprintf (stderr, "%s: only one `-id' option may be used.\n",
		       progname);
	      usage ();
	      exit (-1);
	    }
	  i++;
	  if (argv[i] &&
	      1 == sscanf (argv[i], " %ld %c", &remote_window, &c))
	    ;
	  else if (argv[i] &&
		   1 == sscanf (argv[i], " 0x%lx %c", &remote_window, &c))
	    ;
	  else
	    {
	      fprintf (stderr, "%s: invalid `-id' option \"%s\"\n",
		       progname, argv[i] ? argv[i] : "");
	      usage ();
	      exit (-1);
	    }
	}
    }

  dpy = XOpenDisplay (dpy_string);
  if (! dpy)
    exit (-1);

  if (sync_p)
    XSynchronize (dpy, True);

#ifdef GLOBAL
  return mozilla_remote_commands (dpy, (Window) remote_window,
				 remote_commands);
#else
  exit (mozilla_remote_commands (dpy, (Window) remote_window,
				 remote_commands));
#endif
}

#endif /* STANDALONE */
