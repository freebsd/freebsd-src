/*
 * Copyright 2008,2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/os3support.h"
#include "desktop/browser.h"
#include "amiga/gui.h"
#include "amiga/bitmap.h"
#include "utils/nsoption.h"
#include "content/urldb.h"
#include "desktop/plotters.h"
#include "desktop/thumbnail.h"

#include <proto/graphics.h>
#include <proto/Picasso96API.h>
#include <intuition/intuition.h>
#ifdef __amigaos4__
#include <graphics/blitattr.h>
#include <graphics/composite.h>
#endif
#include <graphics/gfxbase.h>

#include <sys/param.h>

bool thumbnail_create(hlcache_handle *content, struct bitmap *bitmap,
	nsurl *url)
{
	struct BitScaleArgs bsa;
	int plot_width;
	int plot_height;
	int redraw_tile_size = nsoption_int(redraw_tile_size_x);
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &amiplot
	};

	if(nsoption_int(redraw_tile_size_y) < nsoption_int(redraw_tile_size_x))
		redraw_tile_size = nsoption_int(redraw_tile_size_y);

	plot_width = MIN(content_get_width(content), redraw_tile_size);
	plot_height = ((plot_width * bitmap->height) + (bitmap->width / 2)) /
			bitmap->width;

	bitmap->nativebm = p96AllocBitMap(bitmap->width, bitmap->height, 32,
							BMF_CLEAR | BMF_DISPLAYABLE | BMF_INTERLEAVED,
							browserglob.bm, RGBFB_A8R8G8B8);

	bitmap->nativebmwidth = bitmap->width;
	bitmap->nativebmheight = bitmap->height;
	ami_clearclipreg(&browserglob);

	thumbnail_redraw(content, plot_width, plot_height, &ctx);

	if(GfxBase->LibNode.lib_Version >= 53) // AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1)
	{
		float resample_scale = bitmap->width / (float)plot_width;
		uint32 flags = COMPFLAG_IgnoreDestAlpha;
		if(nsoption_bool(scale_quality)) flags |= COMPFLAG_SrcFilter;

		CompositeTags(COMPOSITE_Src,browserglob.bm,bitmap->nativebm,
					COMPTAG_ScaleX,
					COMP_FLOAT_TO_FIX(resample_scale),
					COMPTAG_ScaleY,
					COMP_FLOAT_TO_FIX(resample_scale),
					COMPTAG_Flags,flags,
					COMPTAG_DestX,0,
					COMPTAG_DestY,0,
					COMPTAG_DestWidth,bitmap->width,
					COMPTAG_DestHeight,bitmap->height,
					COMPTAG_OffsetX,0,
					COMPTAG_OffsetY,0,
					COMPTAG_FriendBitMap, scrn->RastPort.BitMap,
					TAG_DONE);
	}
	else
	{
		bsa.bsa_SrcX = 0;
		bsa.bsa_SrcY = 0;
		bsa.bsa_SrcWidth = plot_width;
		bsa.bsa_SrcHeight = plot_height;
		bsa.bsa_DestX = 0;
		bsa.bsa_DestY = 0;
	//	bsa.bsa_DestWidth = width;
	//	bsa.bsa_DestHeight = height;
		bsa.bsa_XSrcFactor = plot_width;
		bsa.bsa_XDestFactor = bitmap->width;
		bsa.bsa_YSrcFactor = plot_height;
		bsa.bsa_YDestFactor = bitmap->height;
		bsa.bsa_SrcBitMap = browserglob.bm;
		bsa.bsa_DestBitMap = bitmap->nativebm;
		bsa.bsa_Flags = 0;

		BitMapScale(&bsa);
	}

	if (url) urldb_set_thumbnail(url, bitmap);

	return true;
}
