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
#include "utils/log.h"
}
#include "beos/throbber.h"
#include "beos/bitmap.h"
#include "beos/fetch_rsrc.h"

#include <File.h>
#include <Resources.h>
#include <TranslationUtils.h>

struct nsbeos_throbber *nsbeos_throbber = NULL;

/**
 * Creates the throbber using a PNG for each frame.  The number of frames must
 * be at least two.  The first frame is the inactive frame, others are the
 * active frames.
 *
 * \param  frames  The number of frames.  Must be at least two.
 * \param  ...     Filenames of PNGs containing frames.
 * \return true on success.
 */
bool nsbeos_throbber_initialise_from_png(const int frames, ...)
{
	va_list filenames;
	struct nsbeos_throbber *throb;		/**< structure we generate */
	bool errors_when_loading = false;	/**< true if a frame failed */
	
	if (frames < 2) {
		/* we need at least two frames - one for idle, one for active */
		LOG(("Insufficent number of frames in throbber animation!"));
		LOG(("(called with %d frames, where 2 is a minimum.)",
			frames));
		return false;
	}

	BResources *res = get_app_resources();
	if (res == NULL) {
		LOG(("Can't find resources for throbber!"));
		return false;
	}

	throb = (struct nsbeos_throbber *)malloc(sizeof(throb));
	throb->nframes = frames;
	throb->framedata = (BBitmap **)malloc(sizeof(BBitmap *) * throb->nframes);
	
	va_start(filenames, frames);
	
	for (int i = 0; i < frames; i++) {
		const char *fn = va_arg(filenames, const char *);
		const void *data;
		size_t size;
		data = res->LoadResource('data', fn, &size);
		throb->framedata[i] = NULL;
		if (!data) {
			LOG(("Error when loading resource %s", fn));
			errors_when_loading = true;
			continue;
		}
		BMemoryIO mem(data, size);
		throb->framedata[i] = BTranslationUtils::GetBitmap(&mem);
		if (throb->framedata[i] == NULL) {
			LOG(("Error when loading %s: GetBitmap() returned NULL", fn));
			errors_when_loading = true;
		}
	}
	
	va_end(filenames);
	
	if (errors_when_loading == true) {
		for (int i = 0; i < frames; i++) {
			delete throb->framedata[i];
		}

		free(throb->framedata);
		free(throb);
		
		return false;		
	}
	
	nsbeos_throbber = throb;
	
	return true;
}

void nsbeos_throbber_finalise(void)
{
	int i;

	for (i = 0; i < nsbeos_throbber->nframes; i++)
		delete nsbeos_throbber->framedata[i];

	free(nsbeos_throbber->framedata);
	free(nsbeos_throbber);

	nsbeos_throbber = NULL;
}
