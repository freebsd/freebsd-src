/*
 * Copyright 2013 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AMIGA_HELP_H
#define AMIGA_HELP_H
#include <exec/types.h>

/* This enum needs to match context_array in help.c */
enum {
	AMI_HELP_MAIN,
	AMI_HELP_GUI,
	AMI_HELP_PREFS,
};

struct Screen;

void ami_help_open(ULONG node, struct Screen *screen);
void ami_help_free(void);
void ami_help_new_screen(struct Screen *screen);
ULONG ami_help_signal(void);
void ami_help_process(void);
#endif

