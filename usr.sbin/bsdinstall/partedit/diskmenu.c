/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Alfonso Sabato Siciliano
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

#include <bsddialog.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diskmenu.h"

int
diskmenu_show(const char *title, const char *text, struct partedit_item *items,
    int nitems, int *focusitem)
{
	int i, output;
	char size[16], *mp;
	struct bsddialog_menuitem *bsditems;
	struct bsddialog_conf conf;

	bsditems = malloc(nitems * sizeof(struct bsddialog_menuitem));
	if (bsditems == NULL)
		return BSDDIALOG_ERROR;
	for (i = 0; i < nitems; i++) {
		bsditems[i].prefix = "";
		bsditems[i].on = false;
		bsditems[i].depth = 2 * items[i].indentation;
		/* old menu sets max namelen to 10 */
		bsditems[i].name = items[i].name;
		humanize_number(size, 7, items[i].size, "B",
		    HN_AUTOSCALE, HN_DECIMAL);
		mp = items[i].mountpoint != NULL ? items[i].mountpoint : "";
		asprintf(__DECONST(char**, &bsditems[i].desc),
		    "  %-9s %-15s %s", size, items[i].type, mp);
		bsditems[i].bottomdesc = "";
	}

	bsddialog_initconf(&conf);
	conf.title = title;
	conf.menu.align_left = true;
	conf.text.escape = true;
	conf.key.f1_message="[\\Z1\\ZbC\\Znreate]: a new partition.\n"
		"[\\Z1\\ZbD\\Znelete]: selected partition(s).\n"
		"[\\Z1\\ZbM\\Znodify]: partition type or mountpoint.\n"
		"[\\Z1\\ZbR\\Znevert]: changes to disk setup.\n"
		"[\\Z1\\ZbA\\Znuto]:   guided partitioning tool.\n"
		"[\\Z1\\ZbF\\Zninish]: will ask to apply changes.";
	conf.menu.shortcut_buttons = true;
	conf.button.ok_label       = "Create";
	conf.button.with_extra     = true;
	conf.button.extra_label    = "Delete";
	conf.button.cancel_label   = "Modify";
	conf.button.with_help      = true;
	conf.button.help_label     = "Revert";
	conf.button.right1_label = "Auto";
	conf.button.right2_label = "Finish";
	conf.button.default_label  = "Finish";
	output = bsddialog_menu(&conf, text, 20, 0, 10, nitems, bsditems,
	    focusitem);

	for (i = 0; i < nitems; i++)
		free((char *)bsditems[i].desc);
	free(bsditems);

	return (output);
}
