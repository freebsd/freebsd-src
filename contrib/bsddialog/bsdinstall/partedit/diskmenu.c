/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alfonso Sabato Siciliano
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
#include <stdio.h>
#include <libutil.h>
#include <stdlib.h>
#include <string.h>

#include "diskmenu.h"

int
diskmenu_show(const char *title, const char *text, struct partedit_item *items,
    int nitems, int *selected)
{
	int i, output;
	struct bsddialog_conf conf;
	struct bsddialog_menuitem *bsditems;
	char desc[1024], *mp, size[16];

	bsditems = malloc(nitems * sizeof(struct bsddialog_menuitem));
	if (bsditems == NULL)
		return BSDDIALOG_ERROR;
	for (i=0; i<nitems; i++) {
		bsditems[i].prefix = "";
		bsditems[i].depth = items[i].indentation;
		bsditems[i].name = __DECONST(char *, items[i].name);
		humanize_number(size, 7, items[i].size, "B",
		    HN_AUTOSCALE, HN_DECIMAL);
		mp = items[i].mountpoint != NULL ? items[i].mountpoint : "";
		snprintf(desc, 1024, "%s %-15s %-10s",
		    size, items[i].type, mp);
		bsditems[i].desc = strdup(desc);
		bsditems[i].bottomdesc = "";
		bsditems[i].on = false;
	}

	bsddialog_initconf(&conf);
	conf.title = __DECONST(char *, title);
	conf.menu.align_left = true;
	/*
	 * libbsddialog does not provides bottom description for buttons.
	 * "Add a new partition", "Delete selected partition or partitions",
	 * "Change partition type or mountpoint",
	 * "Revert changes to disk setup", "Use guided partitioning tool",
	 * "Exit partitioner (will ask whether to save changes)",
	 */
	conf.button.ok_label       = "Create";	
	conf.button.extra_button   = true;
	conf.button.extra_label    = "Delete";
	conf.button.no_label       = "Modify";
	conf.button.help_button    = true;
	conf.button.help_label     = "Revert";
	conf.button.generic1_label = "Auto";
	conf.button.generic2_label = "Finish";
	conf.button.default_label  = "Finish";
	
	if (bsddialog_init() == BSDDIALOG_ERROR)
		return BSDDIALOG_ERROR;

	output = bsddialog_menu(&conf, __DECONST(char *, text), 20, 0, 10,
	    nitems, bsditems, NULL);
	bsddialog_end();

	for (i=0; i<nitems; i++){
		if (bsditems[i].on == true) {
			*selected = i;
			break;
		}
	}

	free(bsditems);
	
	return output;
}
