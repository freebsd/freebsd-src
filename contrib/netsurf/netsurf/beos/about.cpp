/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

#define __STDBOOL_H__	1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "desktop/netsurf.h"
#include "utils/log.h"
#include "utils/testament.h"
#include "utils/useragent.h"
#include "curl/curlver.h"
}
#include "beos/about.h"
#include "beos/scaffolding.h"
#include "beos/window.h"

#include <Alert.h>
#include <Application.h>
#include <Invoker.h>
#include <String.h>


/**
 * Creates the about alert
 */
void nsbeos_about(struct gui_window *gui)
{
	BString text;
	text << "Netsurf  : " << user_agent_string() << "\n";
	text << "Version  : " << netsurf_version << "\n";
	text << "Build ID : " << WT_REVID << "\n";
	text << "Date     : " << WT_COMPILEDATE << "\n";
	text << "cURL     : " << LIBCURL_VERSION << "\n";

	BAlert *alert = new BAlert("about", text.String(), "Credits", "Licence", "Ok");

	BHandler *target = be_app;
	BMessage *message = new BMessage(ABOUT_BUTTON);
	BInvoker *invoker = NULL;
	if (gui) {
		nsbeos_scaffolding *s = nsbeos_get_scaffold(gui);
		if (s) {
			NSBrowserWindow *w = nsbeos_get_bwindow_for_scaffolding(s);
			if (w) {
				alert->SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
				alert->AddToSubset(w);
			}
			NSBaseView *v = nsbeos_get_baseview_for_scaffolding(s);
			if (v) {
				if (w)
					message->AddPointer("Window", w);
				target = v;
			}
		}
	}
	invoker = new BInvoker(message, target);

	//TODO: i18n-ize

	alert->Go(invoker);
}
