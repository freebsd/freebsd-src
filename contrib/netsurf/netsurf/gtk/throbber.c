/*
 * Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef WITH_GIF
#include <libnsgif.h>
#endif
#include "utils/log.h"
#include "gtk/throbber.h"
#include "gtk/bitmap.h"

struct nsgtk_throbber *nsgtk_throbber = NULL;

/**
 * Creates the throbber using a PNG for each frame.  The number of frames must
 * be at least two.  The first frame is the inactive frame, others are the
 * active frames.
 *
 * \param  frames  The number of frames.  Must be at least two.
 * \param  ...     Filenames of PNGs containing frames.
 * \return true on success.
 */
bool nsgtk_throbber_initialise_from_png(const int frames, char** frame_files)
{
	GError *err = NULL;
	struct nsgtk_throbber *throb;		/**< structure we generate */
	bool errors_when_loading = false;	/**< true if a frame failed */
	int frame_loop;
	
	if (frames < 2) {
		/* we need at least two frames - one for idle, one for active */
		LOG(("Insufficent number of frames in throbber animation!"));
		LOG(("(called with %d frames, where 2 is a minimum.)",
			frames));
		return false;
	}
	
	throb = malloc(sizeof(*throb));
	if (throb == NULL)
		return false;

	throb->nframes = frames;
	throb->framedata = malloc(sizeof(GdkPixbuf *) * throb->nframes);
	if (throb->framedata == NULL) {
		free(throb);
		return false;
	}
	
	for (frame_loop = 0; frame_loop < frames; frame_loop++) {
		throb->framedata[frame_loop] = gdk_pixbuf_new_from_file(frame_files[frame_loop], &err);
		if (err != NULL) {
			LOG(("Error when loading %s: %s (%d)",
				frame_files[frame_loop], err->message, err->code));
			throb->framedata[frame_loop] = NULL;
			errors_when_loading = true;
		}
	}
	
	if (errors_when_loading == true) {
		for (frame_loop = 0; frame_loop < frames; frame_loop++) {
			if (throb->framedata[frame_loop] != NULL)
				g_object_unref(throb->framedata[frame_loop]);
		}

		free(throb->framedata);
		free(throb);
		
		return false;		
	}
	
	nsgtk_throbber = throb;
	
	return true;
}

void nsgtk_throbber_finalise(void)
{
	int i;

	for (i = 0; i < nsgtk_throbber->nframes; i++)
		g_object_unref(nsgtk_throbber->framedata[i]);

	free(nsgtk_throbber->framedata);
	free(nsgtk_throbber);

	nsgtk_throbber = NULL;
}

