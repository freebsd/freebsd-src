/*-
 * This code is derived from software copyrighted by the Free Software
 * Foundation.
 *
 * Modified 1991 by Donn Seeley at UUNET Technologies, Inc.
 * Modified 1990 by Van Jacobson at Lawrence Berkeley Laboratory.
 *
 * static char rcsid[] = "$Header: /home/cvs/386BSD/src/usr.bin/gdb/xgdb/xgdb.c,v 1.1.1.1 1993/06/12 14:52:36 rgrimes Exp $";
 */

#ifndef lint
static char sccsid[] = "@(#)xgdb.c	6.3 (Berkeley) 5/8/91";
#endif /* not lint */

/*
 * Interface from GDB to X windows. Copyright (C) 1987 Free Software
 * Foundation, Inc.
 * 
 * GDB is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY.  No author or distributor accepts responsibility to anyone for
 * the consequences of using it or for whether it serves any particular
 * purpose or works at all, unless he says so in writing. Refer to the GDB
 * General Public License for full details.
 * 
 * Everyone is granted permission to copy, modify and redistribute GDB, but only
 * under the conditions described in the GDB General Public License.  A copy
 * of this license is supposed to have been given to you along with GDB so
 * you can know your rights and responsibilities.  It should be in a file
 * named COPYING.  Among other things, the copyright notice and this notice
 * must be preserved on all copies.
 * 
 * In other words, go ahead and share GDB, but don't try to stop anyone else
 * from sharing it farther.  Help stamp out software hoarding!
 */

/*
 * Original version was contributed by Derek Beatty, 30 June 87.
 * This version is essentially a re-write of the original by Van
 * Jacobson (van@helios.ee.lbl.gov), Nov, 90.
 */

#include "defs.h"
#include "param.h"
#include "symtab.h"
#include "frame.h"

extern int stop_breakpoint;

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/AsciiSink.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Text.h>

#include <stdio.h>
#include <ctype.h>
#include <sys/file.h>
#include <sys/errno.h>

extern int errno;
extern char *getenv();
extern char *malloc();
extern void bcopy();
extern int select();

extern int get_filename_and_charpos();
extern int source_line_charpos();
extern int source_charpos_line();
extern void execute_command();
extern void error_no_arg();
extern void add_com();

/* The X display where the window appears.  */

static char *displayname;
static Display *display;

static XtAppContext app_context;

/* Windows manipulated by this package.  */

static Widget main_widget;
static Widget containing_widget;
static Widget source_name_widget;
static Widget source_text_widget;
static Widget button_box_widget;

/* Source text display.  */

static struct frame_info *last_fi;
static CORE_ADDR last_pc;
static struct symtab *last_cur_symtab;
static int last_cur_line;

static int source_window_line;
static char *source_window_file;
static struct symtab *source_window_symtab;

static char version_label[64];
extern char *version;

/* Forward declarations */

static Widget create_text_widget();

static int
safe_strcmp(a, b)
	register char *a, *b;
{
	register int i;

	if (a == b)
		return (0);
	if (!a && b)
		return (1);
	if (a && !b)
		return (-1);
	return (strcmp(a, b));
}


/* Display an appropriate piece of source code in the source window.  */

void
xgdb_display_source()
{
	char *filename = NULL;
	struct symtab_and_line get_selected_frame_sal();
	struct symtab_and_line sal;
	struct frame_info *fi;

	/* Do nothing if called before we are initialized */

	if (!containing_widget)
		return;

	/*
	 * Figure out what to display (the appropriate hooks to tell
	 * us don't exist so we guess):  If there's a current frame
	 * and it or its pc changed from the last time we were here,
	 * display appropriate source line.  Otherwise if the current
	 * source symtab or line is different, display that line.
	 * Otherwise nothing changed so leave the display alone.
	 */
	fi = get_frame_info(selected_frame);
	if (fi && (fi != last_fi || fi->pc != last_pc)) {
		last_fi = fi;
		last_pc = fi->pc;
		sal = find_pc_line(fi->pc, fi->next_frame);
		if (sal.symtab == NULL) {	/* XXX */
			sal.symtab = current_source_symtab;
			sal.line = current_source_line;
		}
		current_source_symtab = sal.symtab;
		current_source_line = sal.line;
	} else if (current_source_symtab != last_cur_symtab ||
		   current_source_line != last_cur_line) {
		sal.symtab = last_cur_symtab = current_source_symtab;
		sal.line = last_cur_line = current_source_line;
	} else
		return;
	/*
	 * Do a path search and get the exact filename of this source file.
	 * Also scan it and find its source lines if not already done.
	 */
	if (sal.symtab && filename == NULL) {
		if (get_filename_and_charpos(sal.symtab, sal.line, &filename))
			/* line numbers may have changed - force highlight */
			source_window_line = -1;
	}

	/*
	 * If the source window is wrong, destroy it and make a new one.
	 */
	if (safe_strcmp(filename, source_window_file)) {
		Arg args[1];
		Widget src = XawTextGetSource(source_text_widget);

		if (filename) {
			XtSetArg(args[0], XtNstring, filename);
			XtSetValues(src, args, XtNumber(args));
			args[0].name = XtNlabel;
			XtSetValues(source_name_widget, args, XtNumber(args));
		} else {
			XtSetArg(args[0], XtNstring, "/dev/null");
			XtSetValues(src, args, XtNumber(args));
			XtSetArg(args[0], XtNlabel, "");
			XtSetValues(source_name_widget, args, XtNumber(args));
		}
		if (source_window_file)
			free(source_window_file);
		source_window_file = filename;
		source_window_line = sal.line + 1;  /* force highlight */
	}
	if (sal.symtab && source_window_line != sal.line) {
		/*
		 * Update display and cursor positions as necessary.
		 * Cursor should be placed on line sal.line.
		 */
		XawTextPosition l, r;

		source_window_symtab = sal.symtab;
		source_window_line = sal.line;
		l = source_line_charpos(source_window_symtab, sal.line);
		r = source_line_charpos(source_window_symtab, sal.line + 1);
		if (r < l)
			r = l + 1;
		XawTextSetSelection(source_text_widget, l, r);
		XawTextScrollToLine(source_text_widget, l, 10, 3);
		XawTextSetInsertionPoint(source_text_widget, l);
	}
}


/*
 * Handlers for buttons.
 */

static int
current_lineno()
{
	XawTextPosition start, finish;

	XawTextGetSelectionPos(source_text_widget, &start, &finish);
	if (start >= finish)
		start = XawTextGetInsertionPoint(source_text_widget);

	return (source_charpos_line(source_window_symtab, start));
}

static char *
append_selection(cp)
	char *cp;
{
	int len;
	XawTextPosition l, r;

	XawTextGetSelectionPos(source_text_widget, &l, &r);
	if ((len = r - l) > 0) {
		Widget src = XawTextGetSource(source_text_widget);

		while (len > 0) {
			XawTextBlock tb;

			XawTextSourceRead(src, l, &tb, len);
			bcopy(tb.ptr, cp, tb.length);
			cp += tb.length;
			len -= tb.length;
		}
		if (cp[-1] == 0)
			--cp;
	}
	return (cp);
}

static char *
append_selection_word(cp)
	register char *cp;
{
	register int len;
	XawTextPosition l, r;
	XawTextBlock tb;
	register char c;
	register Widget src = XawTextGetSource(source_text_widget);

	XawTextGetSelectionPos(source_text_widget, &l, &r);
	if ((len = r - l) <= 0) {
		l = XawTextGetInsertionPoint(source_text_widget);
		len = 128;	/* XXX */

		/* might have clicked in middle of word -- back up to start */
		for ( ; l > 0; --l) {
			XawTextSourceRead(src, l - 1, &tb, 1);
			c = tb.ptr[0];
			if (! isalnum(c) && c != '_' && c != '$')
				break;
		}
	}
	while (len > 0) {
		char *sp;
		int i;

		XawTextSourceRead(src, l, &tb, len);
		for (sp = tb.ptr, i = tb.length; --i >= 0; ) {
			c = *sp++;
			if (!isalnum(c) && c != '_' && c != '$')
				return (cp);
			*cp++ = c;
		}
		len -= tb.length;
	}
	return (cp);
}

static char *
append_selection_expr(cp)
	char *cp;
{
	int len;
	XawTextPosition l, r;
	Widget src = XawTextGetSource(source_text_widget);
	XawTextBlock tb;
	char *sp;
	char c;

	XawTextGetSelectionPos(source_text_widget, &l, &r);
	if (r > l)
		return (append_selection(cp));

	l = XawTextGetInsertionPoint(source_text_widget);

	/* might have clicked in middle of word -- back up to start */
	for ( ; l > 0; --l) {
		XawTextSourceRead(src, l - 1, &tb, 1);
		c = tb.ptr[0];
		if (! isalnum(c) && c != '_' && c != '$')
			break;
	}

	len = 128;	/* XXX */
	while (len > 0) {
		int i;
		char pstack[64];
		int pcnt = 0;

		XawTextSourceRead(src, l, &tb, len);
		for (sp = tb.ptr, i = tb.length; --i >= 0; ) {
			switch (c = *sp++) {
			case '\n':
			case ';':
				return (cp);
			case '=':
				if (cp[-1] != '=')
					return (cp - 1);
				if (len == 128)
					return (cp);
				break;
			case ',':
				if (pcnt <= 0)
					return (cp);
				break;
			case '(':
				pstack[pcnt] = ')';
				if (++pcnt >= sizeof(pstack))
					return (cp);
				break;
			case '[':
				pstack[pcnt] = ']';
				if (++pcnt >= sizeof(pstack))
					return (cp);
				break;
			case ')':
			case ']':
				if (--pcnt < 0 || pstack[pcnt] != c)
					return (cp);
				break;
			}
			*cp++ = c;
		}
		len -= tb.length;
	}
	return (cp);
}

static int input_avail;	/* XXX kluge: do_command sets this when command
			 * data from button is avaialble to force top level
			 * to break out of its loop. */
/*
 * Handle a button by running the command COMMAND.
 */
static void
do_command(w, command, call_data)
	Widget w;
	register char *command;
	caddr_t call_data;
{
	char cmd_line[256];
	char buf[256];
	register char *out = cmd_line;
	char *cp;
	register char c;
	extern char *finish_command_input();

	while (c = *command++) {
		if (c == '%') {
			switch (*command++) {
			case 's':	/* current selection */
				out = append_selection(out);
				break;
			case 'S':	/* 1st selected "word" at curor */
				out = append_selection_word(out);
				break;
			case 'e':	/* echo cmd before executing */
				break;
			case 'E':	/* 1st selected expression at curor */
				out = append_selection_expr(out);
				break;
				
			case 'l':	/* current line number */
				(void) sprintf(buf, "%d", current_lineno());
				for (cp = buf; c = *cp++; *out++ = c)
					;
				break;
			case 'L':	/* line we're stopped at */
				(void) sprintf(buf, "%d", source_window_line);
				for (cp = buf; c = *cp++; *out++ = c)
					;
				break;
			case 'f':	/* current file name */
				for (cp = source_window_symtab->filename;
				     c = *cp++; *out++ = c)
					;
				break;
			case 'b':	/* break # we're stopped at */
				if (stop_breakpoint <= 0)
					/* if no breakpoint, don't do cmd */
					return;

				(void) sprintf(buf, "%d", stop_breakpoint);
				for (cp = buf; c = *cp++; *out++ = c)
					;
				break;
			}
		} else
			*out++ = c;
	}
	*out = 0;
	reinitialize_more_filter();
	/* have to exit via readline or tty modes stay messed up */
	for (cp = cmd_line; c = *cp++; )
		rl_stuff_char(c);
	rl_stuff_char('\n');
	input_avail = 1;
}

/*
 * Define and display all the buttons.
 */
static void
addbutton(parent, name, function, closure)
	Widget parent;
	char *name;
	void (*function) ();
	caddr_t closure;
{
	static XtCallbackRec Callback[] = {
		{NULL, (caddr_t) NULL},
		{NULL, (caddr_t) NULL},
	};
	static Arg commandArgs[] = {
		{XtNlabel, (XtArgVal) NULL},
		{XtNcallback, (XtArgVal) Callback},
	};
	Widget w;
	char wname[128];
	register char *cp;

	strcpy(wname, name);
	while ((cp = index(wname, '*')) || (cp = index(wname, '.')))
		*cp -= 0x10;

	if (w = XtNameToWidget(parent, wname))
		XtDestroyWidget(w);

	Callback[0].callback = (XtCallbackProc) function;
	Callback[0].closure = (caddr_t) closure;
	commandArgs[0].value = (XtArgVal) name;
	XtCreateManagedWidget(wname, commandWidgetClass, parent,
			      commandArgs, XtNumber(commandArgs));
}

/*
 * Create the button windows and store them in `buttons'.
 */
static void
create_buttons(parent)
	Widget parent;
{
	addbutton(parent, "quit", do_command, "quit");
}

static void
button_command(arg)
	char *arg;
{
	char *label;
	unsigned int len;
	
	if (! arg)
		error_no_arg("button label and command");

	for (len = strlen(arg); len > 0 && isspace(arg[len - 1]); --len)
		;
	if (len == 0)
		error_no_arg("button label and command");
	arg[len] = 0;

	/* make a copy of button label & command for toolkit to use */
	label = malloc(len + 1);
	strcpy(label, arg);

	/* find the end of the label */
	if (*label == '"') {
		if ((arg = index(++label, '"')) == 0) {
			printf("button label missing closing quote\n");
			return;
		}
		*arg++ = 0;
	} else if (arg = index(label, ' '))
		*arg++ = 0;
	else
		arg = label;

	while (*arg && isspace(*arg))
		++arg;
	
	addbutton(button_box_widget, label, do_command, arg);
}

static void
button_delete_command(arg)
	char *arg;
{
	unsigned int len;
	Widget w;
	register char *cp;

	if (! arg)
		error_no_arg("button name");

	for (len = strlen(arg); len > 0 && isspace(arg[len - 1]); --len)
		;
	if (len == 0)
		error_no_arg("button name");
	arg[len] = 0;

	/* find the end of the label */
	if (*arg == '"') {
		if ((cp = index(++arg, '"')) == 0) {
			printf("button label missing closing quote\n");
			return;
		}
		*cp++ = 0;
	}
	while ((cp = index(arg, '*')) || (cp = index(arg, '.')))
		*cp -= 0x10;

	if (w = XtNameToWidget(button_box_widget, arg))
		XtDestroyWidget(w);
}

/*
 * Create a "label window" that just displays the string LABEL.
 */
static Widget
create_label(name, label)
	char *name, *label;
{
	Arg args[1];
	Widget w;

	XtSetArg(args[0], XtNlabel, label);
	w = XtCreateManagedWidget(name, labelWidgetClass, containing_widget,
				  args, XtNumber(args));
	return (w);
}

/*
 * Create a subwindow of PARENT that displays and scrolls the contents of
 * file FILENAME.
 */
static Widget
create_text_widget(parent, filename)
	Widget parent;
	char *filename;
{
	static Arg arg[] = {
		{XtNstring, NULL},
		{XtNtype, XawAsciiFile},
		{XtNcursor, None},
	};
	Widget text_widget;

	arg[0].value = (XtArgVal)filename;
	text_widget = XtCreateManagedWidget("src", asciiTextWidgetClass,
					     parent, arg, XtNumber(arg));
	return (text_widget);
}

/*
 * Entry point to create the widgets representing our display.
 */
void
xgdb_create_window()
{
	/* initialize toolkit, setup defaults */
#ifdef notyet
	main_widget = XtAppInitialize(&app_context, "Xgdb", NULL, 0, 
				      argcptr, argv, NULL, NULL, 0);
#else
	char *dummy_argv[] = { "xgdb", 0 };
	int dummy_argc = 1;
	main_widget = XtAppInitialize(&app_context, "Xgdb", NULL, 0, 
				      &dummy_argc, dummy_argv, NULL, NULL, 0);
#endif
	display = XtDisplay(main_widget);
	containing_widget = XtCreateManagedWidget("frame", panedWidgetClass,
					          main_widget, NULL, 0);

	sprintf(version_label, "XGDB %s", version);
	button_box_widget = XtCreateManagedWidget("buttons", boxWidgetClass,
						  containing_widget, NULL, 0);
	create_buttons(button_box_widget);
	source_name_widget = create_label("srcLabel", "No source file yet.");
	source_text_widget = create_text_widget(containing_widget, "/dev/null");

	XtRealizeWidget(main_widget);
	XFlush(display);
}

/*
 * If we use an X window, the readline input loop is told to call
 * this function before reading a character from stdin.
 */
/*ARGSUSED*/
static void
xgdb_window_hook()
{
	register int inmask = 1 << fileno(stdin);
	register int xmask = 1 << ConnectionNumber(display);
	register int nfds, pend;
	int input_rfds;
	XEvent ev;

	/*
	 * Display our current idea of the `interesting' source file then
	 * loop, dispatching window events until data is available on
	 * stdin. Then return so the input data can be processed.
	 */
	input_avail = 0;
	xgdb_display_source();

	input_rfds = 0;
	while (input_avail == 0 && (input_rfds & inmask) == 0) {
		pend = XPending(display);
		if (!pend) {
			input_rfds = inmask | xmask;
			nfds = select(32, &input_rfds, 0, 0,
				      (struct timeval *)0);
			if (nfds == -1 && errno == EINTR)
				continue;
		}
		if (pend || (input_rfds & xmask)) {
			XNextEvent(display, &ev);
			XtDispatchEvent(&ev);
		}
	}
}

void
_initialize_xgdb()
{
	extern void (*window_hook) ();
	extern int inhibit_windows;
	extern struct cmd_list_element *deletelist;

	if (inhibit_windows)
		return;

	if (! displayname) {
		displayname = getenv("DISPLAY");
		if (! displayname) {
			fprintf(stderr, "xgdb: no display name\n");
			inhibit_windows = 1;
			return;
		}
	}
	xgdb_create_window();
	window_hook = xgdb_window_hook;
	add_com("button", class_support, button_command,
"Add command button to xgdb window.  First argument is button\n\
label, second is command associated with button.  Command can\n\
include printf-like escapes:\n\
   %s for current selection,\n\
   %S for first 'word' of current selection,\n\
   %e for current selection or expression at insertion pt,\n\
   %E for current selection or expression at insertion pt,\n\
   %l for current line number,\n\
   %L for line program stopped at,\n\
   %f for current file name,\n\
   %b for current breakpoint number.");
	add_cmd("button", class_support, button_delete_command,
"Delete a button from the xgdb window.\n\
Argument is name of button to be deleted.",
		&deletelist);
}
