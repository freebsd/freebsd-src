/*
 * Copyright 1991 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/*
 * xditview --
 *
 *   Display ditroff output in an X window
 */

#ifndef SABER
#ifndef lint
static char rcsid[] = "$XConsortium: xditview.c,v 1.17 89/12/10 17:05:08 rws Exp $";
#endif /* lint */
#endif /* SABER */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>

#include <signal.h>

#include "Dvi.h"

#include "xdit.bm"
#include "xdit_mask.bm"
#include "stdio.h"

extern FILE *popen();
extern void exit();

static struct app_resources {
    char *print_command;
    char *filename;
} app_resources;

#define offset(field) XtOffset(struct app_resources *, field)

/* Application resources. */

static XtResource resources[] = {
  {"printCommand", "PrintCommand", XtRString, sizeof(char*),
       offset(print_command), XtRString, NULL},
  {"filename", "Filename", XtRString, sizeof(char*),
       offset(filename), XtRString, NULL},
};

#undef offset

/* Command line options table.  Only resources are entered here...there is a
   pass over the remaining options after XtParseCommand is let loose. */

static XrmOptionDescRec options[] = {
{"-page",	    "*dvi.pageNumber",	    XrmoptionSepArg,	NULL},
{"-backingStore",   "*dvi.backingStore",    XrmoptionSepArg,	NULL},
{"-resolution",	    "*dvi.resolution",      XrmoptionSepArg,	NULL},
{"-printCommand",   ".printCommand",        XrmoptionSepArg,	NULL},
{"-filename",       ".filename",            XrmoptionSepArg,	NULL},
{"-noPolyText",	    "*dvi.noPolyText",	    XrmoptionNoArg,	"TRUE"},
};

static char current_print_command[1024];

static char	current_file_name[1024];
static FILE	*current_file;

/*
 * Report the syntax for calling xditview.
 */

static
Syntax(call)
	char *call;
{
	(void) printf ("Usage: %s [-fg <color>] [-bg <color>]\n", call);
	(void) printf ("       [-bd <color>] [-bw <pixels>] [-help]\n");
	(void) printf ("       [-display displayname] [-geometry geom]\n");
	(void) printf ("       [-page <page-number>] [-backing <backing-store>]\n");
	(void) printf ("       [-resolution <res>] [-print <command>]\n");
	(void) printf ("       [-filename <file>] [filename]\n\n");
	exit(1);
}

static void	NewFile (), SetPageNumber ();
static Widget	toplevel, paned, viewport, dvi;
static Widget	page;
static Widget	simpleMenu;

static void	NextPage(), PreviousPage(), SelectPage(), OpenFile(), Quit();
static void	Print();

static struct menuEntry {
    char    *name;
    void    (*function)();
} menuEntries[] = {
    "nextPage",	    NextPage,
    "previousPage", PreviousPage,
    "selectPage",   SelectPage,
    "print",	    Print,
    "openFile",	    OpenFile,
    "quit",	    Quit,
};

static void	NextPageAction(), PreviousPageAction(), SelectPageAction();
static void	OpenFileAction(), QuitAction();
static void	AcceptAction(), CancelAction();
static void	PrintAction();
static void	RerasterizeAction();

XtActionsRec xditview_actions[] = {
    "NextPage",	    NextPageAction,
    "PreviousPage", PreviousPageAction,
    "SelectPage",   SelectPageAction,
    "Print",	    PrintAction,
    "OpenFile",	    OpenFileAction,
    "Rerasterize",  RerasterizeAction,
    "Quit",	    QuitAction,
    "Accept",	    AcceptAction,
    "Cancel",	    CancelAction,
};

#define MenuNextPage		0
#define MenuPreviousPage	1
#define MenuSelectPage		2
#define MenuPrint		3
#define MenuOpenFile		4
#define	MenuQuit		5

static char	pageLabel[256] = "Page <none>";

void main(argc, argv)
    int argc;
    char **argv;
{
    char	    *file_name = 0;
    int		    i;
    static Arg	    labelArgs[] = {
			{XtNlabel, (XtArgVal) pageLabel},
    };
    Arg		    topLevelArgs[2];
    Widget          entry;
    Arg		    pageNumberArgs[1];
    int		    page_number;

    toplevel = XtInitialize("main", "GXditview",
			    options, XtNumber (options),
 			    &argc, argv);
    if (argc > 2)
	Syntax(argv[0]);

    XtGetApplicationResources(toplevel, (XtPointer)&app_resources,
			      resources, XtNumber(resources),
			      NULL, (Cardinal) 0);
    if (app_resources.print_command)
	strcpy(current_print_command, app_resources.print_command);

    XtAppAddActions(XtWidgetToApplicationContext(toplevel),
		    xditview_actions, XtNumber (xditview_actions));

    XtSetArg (topLevelArgs[0], XtNiconPixmap,
	      XCreateBitmapFromData (XtDisplay (toplevel),
				     XtScreen(toplevel)->root,
				     xdit_bits, xdit_width, xdit_height));

    XtSetArg (topLevelArgs[1], XtNiconMask,
	      XCreateBitmapFromData (XtDisplay (toplevel),
				     XtScreen(toplevel)->root,
				     xdit_mask_bits,
				     xdit_mask_width, xdit_mask_height));
    XtSetValues (toplevel, topLevelArgs, 2);
    if (argc > 1)
	file_name = argv[1];

    /*
     * create the menu and insert the entries
     */
    simpleMenu = XtCreatePopupShell ("menu", simpleMenuWidgetClass, toplevel,
				    NULL, 0);
    for (i = 0; i < XtNumber (menuEntries); i++) {
	entry = XtCreateManagedWidget(menuEntries[i].name,
				      smeBSBObjectClass, simpleMenu,
				      NULL, (Cardinal) 0);
	XtAddCallback(entry, XtNcallback, menuEntries[i].function, NULL);
    }

    paned = XtCreateManagedWidget("paned", panedWidgetClass, toplevel,
				    NULL, (Cardinal) 0);
    viewport = XtCreateManagedWidget("viewport", viewportWidgetClass, paned,
				     NULL, (Cardinal) 0);
    dvi = XtCreateManagedWidget ("dvi", dviWidgetClass, viewport, NULL, 0);
    page = XtCreateManagedWidget ("label", labelWidgetClass, paned,
					labelArgs, XtNumber (labelArgs));
    XtSetArg (pageNumberArgs[0], XtNpageNumber, &page_number);
    XtGetValues (dvi, pageNumberArgs, 1);
    if (file_name)
	NewFile (file_name);
    /* NewFile modifies current_file_name, so do this here. */
    if (app_resources.filename)
	strcpy(current_file_name, app_resources.filename);
    XtRealizeWidget (toplevel);
    if (file_name)
	SetPageNumber (page_number);
    XtMainLoop();
}

static void
SetPageNumber (number)
{
    Arg	arg[2];
    int	actual_number, last_page;

    XtSetArg (arg[0], XtNpageNumber, number);
    XtSetValues (dvi, arg, 1);
    XtSetArg (arg[0], XtNpageNumber, &actual_number);
    XtSetArg (arg[1], XtNlastPageNumber, &last_page);
    XtGetValues (dvi, arg, 2);
    if (actual_number == 0)
	sprintf (pageLabel, "Page <none>");
    else if (last_page > 0)
	sprintf (pageLabel, "Page %d of %d", actual_number, last_page);
    else
	sprintf (pageLabel, "Page %d", actual_number);
    XtSetArg (arg[0], XtNlabel, pageLabel);
    XtSetValues (page, arg, 1);
}

static void
SelectPageNumber (number_string)
char	*number_string;
{
	SetPageNumber (atoi(number_string));
}

static int hadFile = 0;

static void
NewFile (name)
char	*name;
{
    Arg	    arg[2];
    char    *n;
    FILE    *new_file;
    Boolean seek = 0;

    if (current_file) {
	if (!strcmp (current_file_name, "-"))
	    ;
	else if (current_file_name[0] == '|')
	    pclose (current_file);
	else
	    fclose (current_file);
    }
    if (!strcmp (name, "-"))
	new_file = stdin;
    else if (name[0] == '|')
	new_file = popen (name+1, "r");
    else {
	new_file = fopen (name, "r");
	seek = 1;
    }
    if (!new_file) {
	/* XXX display error message */
	return;
    }
    XtSetArg (arg[0], XtNfile, new_file);
    XtSetArg (arg[1], XtNseek, seek);
    XtSetValues (dvi, arg, 2);
    if (hadFile || name[0] != '-' || name[1] != '\0') {
	XtSetArg (arg[0], XtNtitle, name);
	if (name[0] != '/' && (n = strrchr (name, '/')))
	    n = n + 1;
	else
	    n = name;
	XtSetArg (arg[1], XtNiconName, n);
	XtSetValues (toplevel, arg, 2);
    }
    hadFile = 1;
    SelectPageNumber ("1");
    strcpy (current_file_name, name);
    current_file = new_file;
}

static char fileBuf[1024];

ResetMenuEntry (entry)
    Widget  entry;
{
    Arg	arg[1];

    XtSetArg (arg[0], XtNpopupOnEntry, entry);
    XtSetValues (XtParent(entry) , arg, (Cardinal) 1);
}

/*ARGSUSED*/

static void
NextPage (entry, name, data)
    Widget  entry;
    caddr_t name, data;
{
    NextPageAction();
    ResetMenuEntry (entry);
}

static void
NextPageAction ()
{
    Arg	args[1];
    int	number;

    XtSetArg (args[0], XtNpageNumber, &number);
    XtGetValues (dvi, args, 1);
    SetPageNumber (number+1);
}

/*ARGSUSED*/

static void
PreviousPage (entry, name, data)
    Widget  entry;
    caddr_t name, data;
{
    PreviousPageAction ();
    ResetMenuEntry (entry);
}

static void
PreviousPageAction ()
{
    Arg	args[1];
    int	number;

    XtSetArg (args[0], XtNpageNumber, &number);
    XtGetValues (dvi, args, 1);
    SetPageNumber (number-1);
}

/* ARGSUSED */

static void
SelectPage (entry, name, data)
    Widget  entry;
    caddr_t name, data;
{
    SelectPageAction ();
    ResetMenuEntry (entry);
}

static void
SelectPageAction ()
{
    MakePrompt (toplevel, "Page number", SelectPageNumber, "");
}


static void
DoPrint (name)
    char *name;
{
    FILE *print_file;
#ifdef SIGNALRETURNSINT
    int (*handler)();
#else
    void (*handler)();
#endif
    /* Avoid dieing because of an invalid command. */
    handler = signal(SIGPIPE, SIG_IGN);

    print_file = popen(name, "w");
    if (!print_file)
	/* XXX print error message */
	return;
    DviSaveToFile(dvi, print_file);
    pclose(print_file);
    signal(SIGPIPE, handler);
    strcpy(current_print_command, name);
}

static void
RerasterizeAction()
{
    Arg	args[1];
    int	number;

    if (current_file_name[0] == 0) {
	/* XXX display an error message */
	return;
    }
    XtSetArg (args[0], XtNpageNumber, &number);
    XtGetValues (dvi, args, 1);
    NewFile(current_file_name);
    SetPageNumber (number);
}

/* ARGSUSED */

static void
Print (entry, name, data)
    Widget  entry;
    caddr_t name, data;
{
    PrintAction ();
    ResetMenuEntry (entry);
}

static void
PrintAction ()
{
    if (current_print_command[0])
	strcpy (fileBuf, current_print_command);
    else
	fileBuf[0] = '\0';
    MakePrompt (toplevel, "Print command:", DoPrint, fileBuf);
}


/* ARGSUSED */

static void
OpenFile (entry, name, data)
    Widget  entry;
    caddr_t name, data;
{
    OpenFileAction ();
    ResetMenuEntry (entry);
}

static void
OpenFileAction ()
{
    if (current_file_name[0])
	strcpy (fileBuf, current_file_name);
    else
	fileBuf[0] = '\0';
    MakePrompt (toplevel, "File to open:", NewFile, fileBuf);
}

/* ARGSUSED */

static void
Quit (entry, closure, data)
    Widget  entry;
    caddr_t closure, data;
{
    QuitAction ();
}

static void
QuitAction ()
{
    exit (0);
}

Widget	promptShell, promptDialog;
void	(*promptfunction)();

/* ARGSUSED */
static
void CancelAction (widget, event, params, num_params)
    Widget	widget;
    XEvent	*event;
    String	*params;
    Cardinal	*num_params;
{
    if (promptShell) {
	XtSetKeyboardFocus(toplevel, (Widget) None);
	XtDestroyWidget(promptShell);
	promptShell = (Widget) 0;
    }
}

static
void AcceptAction (widget, event, params, num_params)
    Widget	widget;
    XEvent	*event;
    String	*params;
    Cardinal	*num_params;
{
    (*promptfunction)(XawDialogGetValueString(promptDialog));
    CancelAction (widget, event, params, num_params);
}

MakePrompt(centerw, prompt, func, def)
Widget	centerw;
char *prompt;
void (*func)();
char	*def;
{
    static Arg dialogArgs[] = {
	{XtNlabel, NULL},
	{XtNvalue, NULL},
    };
    Arg valueArgs[1];
    Arg centerArgs[2];
    Position	source_x, source_y;
    Position	dest_x, dest_y;
    Dimension center_width, center_height;
    Dimension prompt_width, prompt_height;
    Widget  valueWidget;

    CancelAction ((Widget)NULL, (XEvent *) 0, (String *) 0, (Cardinal *) 0);
    promptShell = XtCreatePopupShell ("promptShell", transientShellWidgetClass,
				      toplevel, NULL, (Cardinal) 0);
    dialogArgs[0].value = (XtArgVal)prompt;
    dialogArgs[1].value = (XtArgVal)def;
    promptDialog = XtCreateManagedWidget( "promptDialog", dialogWidgetClass,
		    promptShell, dialogArgs, XtNumber (dialogArgs));
    XawDialogAddButton(promptDialog, "accept", NULL, (caddr_t) 0);
    XawDialogAddButton(promptDialog, "cancel", NULL, (caddr_t) 0);
    valueWidget = XtNameToWidget (promptDialog, "value");
    if (valueWidget) {
    	XtSetArg (valueArgs[0], XtNresizable, TRUE);
    	XtSetValues (valueWidget, valueArgs, 1);
	/*
	 * as resizable isn't set until just above, the
	 * default value will be displayed incorrectly.
	 * rectify the situation by resetting the values
	 */
        XtSetValues (promptDialog, dialogArgs, XtNumber (dialogArgs));
    }
    XtSetKeyboardFocus (promptDialog, valueWidget);
    XtSetKeyboardFocus (toplevel, valueWidget);
    XtRealizeWidget (promptShell);
    /*
     * place the widget in the center of the "parent"
     */
    XtSetArg (centerArgs[0], XtNwidth, &center_width);
    XtSetArg (centerArgs[1], XtNheight, &center_height);
    XtGetValues (centerw, centerArgs, 2);
    XtSetArg (centerArgs[0], XtNwidth, &prompt_width);
    XtSetArg (centerArgs[1], XtNheight, &prompt_height);
    XtGetValues (promptShell, centerArgs, 2);
    source_x = (center_width - prompt_width) / 2;
    source_y = (center_height - prompt_height) / 3;
    XtTranslateCoords (centerw, source_x, source_y, &dest_x, &dest_y);
    XtSetArg (centerArgs[0], XtNx, dest_x);
    XtSetArg (centerArgs[1], XtNy, dest_y);
    XtSetValues (promptShell, centerArgs, 2);
    XtMapWidget(promptShell);
    promptfunction = func;
}

/* For DviChar.c */

char *xmalloc(n)
    int n;
{
    return XtMalloc(n);
}

/*
Local Variables:
c-indent-level: 4
c-continued-statement-offset: 4
c-brace-offset: -4
c-argdecl-indent: 4
c-label-offset: -4
c-tab-always-indent: nil
End:
*/
