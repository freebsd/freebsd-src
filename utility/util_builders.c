/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 Alfonso Sabato Siciliano
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <bsddialog.h>
#include <bsddialog_theme.h>

#include "util.h"

#define NO_PRINT_VALUES(rv)                                                    \
(rv == BSDDIALOG_ERROR || rv == BSDDIALOG_CANCEL || rv == BSDDIALOG_ESC)

/* message */
int infobox_builder(BUILDER_ARGS)
{
	if (argc > 0)
		error_args(opt->name, argc, argv);

	return (bsddialog_infobox(conf, text, rows, cols));
}

int msgbox_builder(BUILDER_ARGS)
{
	if (argc > 0)
		error_args(opt->name, argc, argv);

	return (bsddialog_msgbox(conf, text, rows, cols));
}

int yesno_builder(BUILDER_ARGS)
{
	if (argc > 0)
		error_args(opt->name, argc, argv);

	return (bsddialog_yesno(conf, text, rows, cols));
}

/* textbox */
int textbox_builder(BUILDER_ARGS)
{
	if (argc > 0)
		error_args(opt->name, argc, argv);

	return (bsddialog_textbox(conf, text, rows, cols));
}

/* bar */
int gauge_builder(BUILDER_ARGS)
{
	int output;
	unsigned int perc;

	perc = 0;
	if (argc == 1) {
		perc = (u_int)strtoul(argv[0], NULL, 10);
		perc = perc > 100 ? 100 : perc;
	} else if (argc > 1) {
		error_args(opt->name, argc - 1, argv + 1);
	}

	output = bsddialog_gauge(conf, text, rows, cols, perc, STDIN_FILENO,
	    "XXX", "EOF");

	return (output);
}

int mixedgauge_builder(BUILDER_ARGS)
{
	int output, *minipercs;
	unsigned int i, mainperc, nminibars;
	const char **minilabels;

	if (argc < 1)
		exit_error(true, "%s missing <mainperc>", opt->name);
	if (((argc-1) % 2) != 0)
		exit_error(true,
		    "bad %s pair number [<minilabel> <miniperc>]", opt->name);

	mainperc = (u_int)strtoul(argv[0], NULL, 10);
	mainperc = mainperc > 100 ? 100 : mainperc;
	argc--;
	argv++;

	nminibars  = argc / 2;
	if ((minilabels = calloc(nminibars, sizeof(char*))) == NULL)
		exit_error(false, "Cannot allocate memory for minilabels");
	if ((minipercs = calloc(nminibars, sizeof(int))) == NULL)
		exit_error(false, "Cannot allocate memory for minipercs");

	for (i = 0; i < nminibars; i++) {
		minilabels[i] = argv[i * 2];
		minipercs[i] = (int)strtol(argv[i * 2 + 1], NULL, 10);
	}

	output = bsddialog_mixedgauge(conf, text, rows, cols, mainperc,
	    nminibars, minilabels, minipercs);

	return (output);
}

int pause_builder(BUILDER_ARGS)
{
	int output;
	unsigned int secs;

	if (argc == 0)
		exit_error(true, "--pause missing <seconds>");
	if (argc > 1)
		error_args(opt->name, argc - 1, argv + 1);

	secs = (u_int)strtoul(argv[0], NULL, 10);
	output = bsddialog_pause(conf, text, rows, cols, &secs);

	return (output);
}

int rangebox_builder(BUILDER_ARGS)
{
	int output, min, max, value;

	if (argc < 2)
		exit_error(true, "--rangebox missing <min> <max> [<init>]");
	if (argc > 3)
		error_args("--rangebox", argc - 3, argv + 3);

	min = (int)strtol(argv[0], NULL, 10);
	max = (int)strtol(argv[1], NULL, 10);

	if (argc == 3) {
		value = (int)strtol(argv[2], NULL, 10);
		value = value < min ? min : value;
		value = value > max ? max : value;
	} else
		value = min;

	output = bsddialog_rangebox(conf, text, rows, cols, min, max, &value);
	if (NO_PRINT_VALUES(output) == false)
		dprintf(opt->output_fd, "%d", value);

	return (output);
}

/* date and time */
static int date(BUILDER_ARGS)
{
	int rv;
	unsigned int yy, mm, dd;
	time_t cal;
	struct tm *localtm;
	char stringdate[1024];

	time(&cal);
	localtm = localtime(&cal);
	yy = localtm->tm_year + 1900;
	mm = localtm->tm_mon + 1;
	dd = localtm->tm_mday;

	if (argc > 3) {
		error_args(opt->name, argc - 3, argv + 3);
	} else if (argc == 3) {
		/* lib checks/sets max and min */
		dd = (u_int)strtoul(argv[0], NULL, 10);
		mm = (u_int)strtoul(argv[1], NULL, 10);
		yy = (u_int)strtoul(argv[2], NULL, 10);
	}

	if (strcmp(opt->name, "--datebox") == 0)
		rv = bsddialog_datebox(conf, text, rows, cols, &yy, &mm, &dd);
	else
		rv = bsddialog_calendar(conf, text, rows, cols, &yy, &mm, &dd);
	if (NO_PRINT_VALUES(rv))
		return (rv);

	if (opt->date_fmt != NULL) {
		time(&cal);
		localtm = localtime(&cal);
		localtm->tm_year = yy - 1900;
		localtm->tm_mon = mm - 1;
		localtm->tm_mday = dd;
		strftime(stringdate, 1024, opt->date_fmt, localtm);
		dprintf(opt->output_fd, "%s", stringdate);
	} else if (opt->bikeshed && ~dd & 1) {
		dprintf(opt->output_fd, "%u/%u/%u", dd, mm, yy);
	} else {
		dprintf(opt->output_fd, "%02u/%02u/%u", dd, mm, yy);
	}

	return (rv);
}

int calendar_builder(BUILDER_ARGS)
{
	/* Use height autosizing with rows = 2. Documented in bsddialog(1).
	 *
	 * f_dialog_calendar_size() in bsdconfig/share/dialog.subr:1352
	 * computes height 2 for `dialog --calendar', called by:
	 * 1) f_dialog_input_expire_password() in
	 * bsdconfig/usermgmt/share/user_input.subr:517 and
	 * 2) f_dialog_input_expire_account() in
	 * bsdconfig/usermgmt/share/user_input.subr:660.
	 *
	 * Then use height autosizing with 2 that is min height like dialog.
	 */
	if (rows == 2)
		rows = 0;

	return (date(conf, text, rows, cols, argc, argv, opt));
}

int datebox_builder(BUILDER_ARGS)
{
	return (date(conf, text, rows, cols, argc, argv, opt));
}

int timebox_builder(BUILDER_ARGS)
{
	int output;
	unsigned int hh, mm, ss;
	time_t clock;
	struct tm *localtm;
	char stringtime[1024];

	time(&clock);
	localtm = localtime(&clock);
	hh = localtm->tm_hour;
	mm = localtm->tm_min;
	ss = localtm->tm_sec;

	if (argc > 3) {
		error_args("--timebox", argc - 3, argv + 3);
	} else if (argc == 3) {
		hh = (u_int)strtoul(argv[0], NULL, 10);
		mm = (u_int)strtoul(argv[1], NULL, 10);
		ss = (u_int)strtoul(argv[2], NULL, 10);
	}

	output = bsddialog_timebox(conf, text, rows, cols, &hh, &mm, &ss);
	if (NO_PRINT_VALUES(output))
		return (output);

	if (opt->time_fmt != NULL) {
		time(&clock);
		localtm = localtime(&clock);
		localtm->tm_hour = hh;
		localtm->tm_min = mm;
		localtm->tm_sec = ss;
		strftime(stringtime, 1024, opt->time_fmt, localtm);
		dprintf(opt->output_fd, "%s", stringtime);
	} else if (opt->bikeshed && ~ss & 1) {
		dprintf(opt->output_fd, "%u:%u:%u", hh, mm, ss);
	} else {
		dprintf(opt->output_fd, "%02u:%02u:%02u", hh, mm, ss);
	}

	return (output);
}

/* menu */
static void
get_menu_items(int argc, char **argv, bool setprefix, bool setdepth,
    bool setname, bool setdesc, bool setstatus, bool sethelp,
    unsigned int *nitems, struct bsddialog_menuitem **items, int *focusitem,
    struct options *opt)
{
	unsigned int i, j, sizeitem;

	*focusitem = -1;

	sizeitem = 0;
	sizeitem += setprefix ? 1 : 0;
	sizeitem += setdepth  ? 1 : 0;
	sizeitem += setname   ? 1 : 0;
	sizeitem += setdesc   ? 1 : 0;
	sizeitem += setstatus ? 1 : 0;
	sizeitem += sethelp   ? 1 : 0;
	if ((argc % sizeitem) != 0)
		exit_error(true, "%s bad arguments items number", opt->name);

	*nitems = argc / sizeitem;
	*items = calloc(*nitems, sizeof(struct bsddialog_menuitem));
	if (items == NULL)
		exit_error(false, "%s cannot allocate items", opt->name);

	j = 0;
	for (i = 0; i < *nitems; i++) {
		(*items)[i].prefix = setprefix ? argv[j++] : "";
		(*items)[i].depth = setdepth ?
		    (u_int)strtoul(argv[j++], NULL, 0) : 0;
		(*items)[i].name = setname ? argv[j++] : "";
		(*items)[i].desc = setdesc ? argv[j++] : "";
		if (setstatus) {
			if (strcasecmp(argv[j], "on") == 0)
				(*items)[i].on = true;
			else if (strcasecmp(argv[j], "off") == 0)
				(*items)[i].on = false;
			else
				exit_error(true,
				    "\"%s\" (item %i) invalid status \"%s\"",
				    (*items)[i].name, i+1, argv[j]);
			j++;
		} else
			(*items)[i].on = false;
		(*items)[i].bottomdesc = sethelp ? argv[j++] : "";

		if (opt->item_default != NULL && *focusitem == -1)
			if (strcmp((*items)[i].name, opt->item_default) == 0)
				*focusitem = i;
	}
}

static void
print_menu_items(int output, int nitems, struct bsddialog_menuitem *items,
    int focusitem, struct options *opt)
{
	bool sep, sepbefore, sepafter, sepsecond, toquote, ismenu, ischecklist;
	int i;
	char quotech;
	const char *focusname, *sepstr;

	ismenu = (strcmp(opt->name, "--menu") == 0) ? true : false;
	ischecklist = (strcmp(opt->name, "--checklist") == 0) ? true : false;
	sep = false;
	quotech = opt->item_singlequote ? '\'' : '"';

	if (NO_PRINT_VALUES(output))
		return;

	if (output == BSDDIALOG_HELP) {
		dprintf(opt->output_fd, "HELP ");

		if (focusitem >= 0) {
			focusname = items[focusitem].name;
			if (opt->item_bottomdesc &&
			    opt->help_print_item_name == false)
				focusname = items[focusitem].bottomdesc;

			toquote = false;
			if (strchr(focusname, ' ') != NULL) {
				toquote = opt->item_always_quote;
				if (ismenu == false &&
				    opt->item_output_sepnl == false)
					toquote = true;
			}
			if (toquote) {
				dprintf(opt->output_fd, "%c%s%c",
				    quotech, focusname, quotech);
			} else
				dprintf(opt->output_fd, "%s", focusname);
		}

		if (ismenu || opt->help_print_items == false)
			return;
		sep = true;
	}

	sepbefore = false;
	sepsecond = false;
	if ((sepstr = opt->item_output_sep) == NULL) {
		if (opt->item_output_sepnl)
			sepstr = "\n";
		else {
			sepstr = " ";
			sepsecond = true;
		}
	} else
		sepbefore = true;

	sepafter = false;
	if (opt->item_output_sepnl) {
		sepbefore = false;
		sepafter = true;
	}

	for (i = 0; i < nitems; i++) {
		if (items[i].on == false)
			continue;

		if (sep || sepbefore)
			dprintf(opt->output_fd, "%s", sepstr);
		sep = false;
		if (sepsecond)
			sep = true;

		toquote = false;
		if (strchr(items[i].name, ' ') != NULL) {
			toquote = opt->item_always_quote;
			if (ischecklist && opt->item_output_sepnl == false)
				toquote = true;
		}
		if (toquote)
			dprintf(opt->output_fd, "%c%s%c",
			    quotech, items[i].name, quotech);
		else
			dprintf(opt->output_fd, "%s", items[i].name);

		if (sepafter)
			dprintf(opt->output_fd, "%s", sepstr);
	}
}

int checklist_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1)
		exit_error(true, "--checklist missing <menurows>");
	menurows = (u_int)strtoul(argv[0], NULL, 10);

	get_menu_items(argc-1, argv+1, opt->item_prefix, opt->item_depth, true,
	    true, true, opt->item_bottomdesc, &nitems, &items, &focusitem, opt);

	output = bsddialog_checklist(conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(output, nitems, items, focusitem, opt);
	free(items);

	if (output == BSDDIALOG_HELP && opt->item_bottomdesc)
		output = BSDDIALOG_ITEM_HELP;

	return (output);
}

int menu_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1)
		exit_error(true, "--menu missing <menurows>");
	menurows = (u_int)strtoul(argv[0], NULL, 10);

	get_menu_items(argc-1, argv+1, opt->item_prefix, opt->item_depth, true,
	    true, false, opt->item_bottomdesc, &nitems, &items, &focusitem,
	    opt);

	output = bsddialog_menu(conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(output, nitems, items, focusitem, opt);
	free(items);

	if (output == BSDDIALOG_HELP && opt->item_bottomdesc)
		output = BSDDIALOG_ITEM_HELP;

	return (output);
}

int radiolist_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1)
		exit_error(true, "--radiolist missing <menurows>");
	menurows = (u_int)strtoul(argv[0], NULL, 10);

	get_menu_items(argc-1, argv+1, opt->item_prefix, opt->item_depth, true,
	    true, true, opt->item_bottomdesc, &nitems, &items, &focusitem, opt);

	output = bsddialog_radiolist(conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(output, nitems, items, focusitem, opt);
	free(items);

	if (output == BSDDIALOG_HELP && opt->item_bottomdesc)
		output = BSDDIALOG_ITEM_HELP;

	return (output);
}

int treeview_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int menurows, nitems;
	struct bsddialog_menuitem *items;

	if (argc < 1)
		exit_error(true, "--treeview missing <menurows>");
	menurows = (u_int)strtoul(argv[0], NULL, 10);

	get_menu_items(argc-1, argv+1, opt->item_prefix, true, true, true, true,
	    opt->item_bottomdesc, &nitems, &items, &focusitem, opt);

	conf->menu.no_name = true;
	conf->menu.align_left = true;

	output = bsddialog_radiolist(conf, text, rows, cols, menurows, nitems,
	    items, &focusitem);

	print_menu_items(output, nitems, items, focusitem, opt);
	free(items);

	if (output == BSDDIALOG_HELP && opt->item_bottomdesc)
		output = BSDDIALOG_ITEM_HELP;

	return (output);
}

/* form */
static void
print_form_items(int output, int nitems, struct bsddialog_formitem *items,
    int focusitem, struct options *opt)
{
	int i;
	const char *helpname;

	if (NO_PRINT_VALUES(output))
		return;

	if (output == BSDDIALOG_HELP) {
		dprintf(opt->output_fd, "HELP");
		if (focusitem >= 0) {
			helpname = items[focusitem].label;
			if (opt->item_bottomdesc &&
			    opt->help_print_item_name == false)
				helpname = items[focusitem].bottomdesc;
			dprintf(opt->output_fd, " %s", helpname);
		}
		if(opt->help_print_items == false)
			return;
		dprintf(opt->output_fd, "\n");
	}

	for (i = 0; i < nitems; i++) {
		dprintf(opt->output_fd, "%s\n", items[i].value);
		free(items[i].value);
	}
}

int form_builder(BUILDER_ARGS)
{
	int output, fieldlen, valuelen, focusitem;
	unsigned int i, j, flags, formheight, nitems, sizeitem;
	struct bsddialog_formitem *items;

	if (argc < 1)
		exit_error(true, "--form missing <formheight>");
	formheight = (u_int)strtoul(argv[0], NULL, 10);

	argc--;
	argv++;
	sizeitem = opt->item_bottomdesc ? 9 : 8;
	if (argc % sizeitem != 0)
		exit_error(true, "--form bad number of arguments items");

	nitems = argc / sizeitem;
	if ((items = calloc(nitems, sizeof(struct bsddialog_formitem))) == NULL)
		exit_error(false, "cannot allocate memory for form items");
	j = 0;
	for (i = 0; i < nitems; i++) {
		items[i].label	= argv[j++];
		items[i].ylabel = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xlabel = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].init	= argv[j++];
		items[i].yfield	= (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xfield	= (u_int)strtoul(argv[j++], NULL, 10);

		fieldlen = (int)strtol(argv[j++], NULL, 10);
		items[i].fieldlen = abs(fieldlen);

		valuelen = (int)strtol(argv[j++], NULL, 10);
		items[i].maxvaluelen = valuelen == 0 ? abs(fieldlen) : valuelen;

		flags = (fieldlen < 0 ? BSDDIALOG_FIELDREADONLY : 0);
		items[i].flags = flags;

		items[i].bottomdesc = opt->item_bottomdesc ? argv[j++] : "";
	}

	focusitem = -1;
	output = bsddialog_form(conf, text, rows, cols, formheight, nitems,
	    items, &focusitem);
	print_form_items(output, nitems, items, focusitem, opt);
	free(items);

	if (output == BSDDIALOG_HELP && opt->item_bottomdesc)
		output = BSDDIALOG_ITEM_HELP;

	return (output);
}

int inputbox_builder(BUILDER_ARGS)
{
	int output;
	struct bsddialog_formitem item;

	if (argc > 1)
		error_args("--inputbox", argc - 1, argv + 1);

	item.label	 = "";
	item.ylabel	 = 0;
	item.xlabel	 = 0;
	item.init	 = argc > 0 ? argv[0] : "";
	item.yfield	 = 0;
	item.xfield	 = 0;
	item.fieldlen    = 1;
	item.maxvaluelen = opt->max_input_form;
	item.flags	 = BSDDIALOG_FIELDNOCOLOR;
	item.flags      |= BSDDIALOG_FIELDCURSOREND;
	item.flags      |= BSDDIALOG_FIELDEXTEND;
	item.bottomdesc  = "";

	output = bsddialog_form(conf, text, rows, cols, 1, 1, &item, NULL);
	print_form_items(output, 1, &item, -1, opt);

	return (output);
}

int mixedform_builder(BUILDER_ARGS)
{
	int output, focusitem;
	unsigned int i, j, formheight, nitems, sizeitem;
	struct bsddialog_formitem *items;

	if (argc < 1)
		exit_error(true, "--mixedform missing <formheight>");
	formheight = (u_int)strtoul(argv[0], NULL, 10);

	argc--;
	argv++;
	sizeitem = opt->item_bottomdesc ? 10 : 9;
	if (argc % sizeitem != 0)
		exit_error(true, "--mixedform bad number of arguments items");

	nitems = argc / sizeitem;
	if ((items = calloc(nitems, sizeof(struct bsddialog_formitem))) == NULL)
		exit_error(false, "cannot allocate memory for form items");
	j = 0;
	for (i = 0; i < nitems; i++) {
		items[i].label	     = argv[j++];
		items[i].ylabel      = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xlabel      = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].init	     = argv[j++];
		items[i].yfield	     = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xfield	     = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].fieldlen    = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].maxvaluelen = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].flags       = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].bottomdesc  = opt->item_bottomdesc ? argv[j++] : "";
	}

	focusitem = -1;
	output = bsddialog_form(conf, text, rows, cols, formheight, nitems,
	    items, &focusitem);
	print_form_items(output, nitems, items, focusitem, opt);
	free(items);

	if (output == BSDDIALOG_HELP && opt->item_bottomdesc)
		output = BSDDIALOG_ITEM_HELP;

	return (output);
}

int passwordbox_builder(BUILDER_ARGS)
{
	int output;
	struct bsddialog_formitem item;

	if (argc > 1)
		error_args("--passwordbox", argc - 1, argv + 1);

	item.label	 = "";
	item.ylabel	 = 0;
	item.xlabel	 = 0;
	item.init	 = argc > 0 ? argv[0] : "";
	item.yfield	 = 0;
	item.xfield	 = 0;
	item.fieldlen	 = 1;
	item.maxvaluelen = opt->max_input_form;
	item.flags       = BSDDIALOG_FIELDHIDDEN;
	item.flags      |= BSDDIALOG_FIELDNOCOLOR;
	item.flags      |= BSDDIALOG_FIELDCURSOREND;
	item.flags      |= BSDDIALOG_FIELDEXTEND;
	item.bottomdesc  = "";

	output = bsddialog_form(conf, text, rows, cols, 1, 1, &item, NULL);
	print_form_items(output, 1, &item, -1, opt);

	return (output);
}

int passwordform_builder(BUILDER_ARGS)
{
	int output, fieldlen, valuelen, focusitem;
	unsigned int i, j, flags, formheight, nitems, sizeitem;
	struct bsddialog_formitem *items;

	if (argc < 1)
		exit_error(true, "--passwordform missing <formheight>");
	formheight = (u_int)strtoul(argv[0], NULL, 10);

	argc--;
	argv++;
	sizeitem = opt->item_bottomdesc ? 9 : 8;
	if (argc % sizeitem != 0)
		exit_error(true, "--passwordform bad arguments items number");

	flags = BSDDIALOG_FIELDHIDDEN;
	nitems = argc / sizeitem;
	if ((items = calloc(nitems, sizeof(struct bsddialog_formitem))) == NULL)
		exit_error(false, "cannot allocate memory for form items");
	j = 0;
	for (i = 0; i < nitems; i++) {
		items[i].label	= argv[j++];
		items[i].ylabel = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xlabel = (u_int)strtoul(argv[j++], NULL, 10);
		items[i].init	= argv[j++];
		items[i].yfield	= (u_int)strtoul(argv[j++], NULL, 10);
		items[i].xfield	= (u_int)strtoul(argv[j++], NULL, 10);

		fieldlen = (int)strtol(argv[j++], NULL, 10);
		items[i].fieldlen = abs(fieldlen);

		valuelen = (int)strtol(argv[j++], NULL, 10);
		items[i].maxvaluelen = valuelen == 0 ? abs(fieldlen) : valuelen;

		flags |= (fieldlen < 0 ? BSDDIALOG_FIELDREADONLY : 0);
		items[i].flags = flags;

		items[i].bottomdesc  = opt->item_bottomdesc ? argv[j++] : "";
	}

	focusitem = -1;
	output = bsddialog_form(conf, text, rows, cols, formheight, nitems,
	    items, &focusitem);
	print_form_items(output, nitems, items, focusitem, opt);
	free(items);

	if (output == BSDDIALOG_HELP && opt->item_bottomdesc)
		output = BSDDIALOG_ITEM_HELP;

	return (output);
}
