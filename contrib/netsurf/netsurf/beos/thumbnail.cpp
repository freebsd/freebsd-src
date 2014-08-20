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

/** \file
 * Page thumbnail creation (implementation).
 *
 * Thumbnails are created by setting the current drawing contexts to a BView
 * attached to the BBitmap we are passed, and plotting the page at a small
 * scale.
 */

#define __STDBOOL_H__	1
#include <assert.h>
#include <sys/param.h>
#include <Bitmap.h>
#include <View.h>
extern "C" {
#include "content/content.h"
#include "content/urldb.h"
#include "desktop/plotters.h"
#include "desktop/browser.h"
#include "desktop/thumbnail.h"
#include "image/bitmap.h"
#include "render/font.h"
#include "utils/log.h"
}
#include "beos/scaffolding.h"
#include "beos/plotters.h"
#include "beos/bitmap.h"

// Zeta PRIVATE: in libzeta for now.
extern status_t ScaleBitmap(const BBitmap& inBitmap, BBitmap& outBitmap);

#warning XXX do we need to set bitmap:shadow ?


/**
 * Create a thumbnail of a page.
 *
 * \param  content  content structure to thumbnail
 * \param  bitmap   the bitmap to draw to
 * \param  url      the URL the thumnail belongs to, or NULL
 */
bool thumbnail_create(hlcache_handle *content, struct bitmap *bitmap,
		nsurl *url)
{
	BBitmap *thumbnail;
	BBitmap *small;
	BBitmap *big;
	BView *oldView;
	BView *view;
	BView *thumbView;
	float width;
	float height;
	int big_width;
	int big_height;
	int depth;

	struct redraw_context ctx;
	ctx.interactive = false;
	ctx.background_images = true;
	ctx.plot = &nsbeos_plotters;

	assert(content);
	assert(bitmap);

	thumbnail = nsbeos_bitmap_get_primary(bitmap);
	width = thumbnail->Bounds().Width();
	height = thumbnail->Bounds().Height();
	depth = 32;
	
	big_width = MIN(content_get_width(content), 1024);
	big_height = (int)(((big_width * height) + (width / 2)) / width);

	BRect contentRect(0, 0, big_width - 1, big_height - 1);
	big = new BBitmap(contentRect, B_BITMAP_ACCEPTS_VIEWS, B_RGB32);

	if (big->InitCheck() < B_OK) {
		delete big;
		return false;
	}

	small = new BBitmap(thumbnail->Bounds(), 
		B_BITMAP_ACCEPTS_VIEWS, B_RGB32);

	if (small->InitCheck() < B_OK) {
		delete small;
		delete big;
		return false;
	}

	//XXX: _lock ?
	// backup the current gc
	oldView = nsbeos_current_gc();

	view = new BView(contentRect, "thumbnailer", 
		B_FOLLOW_NONE, B_WILL_DRAW);
	big->AddChild(view);

	thumbView = new BView(small->Bounds(), "thumbnail", 
		B_FOLLOW_NONE, B_WILL_DRAW);
	small->AddChild(thumbView);

	view->LockLooper();

	/* impose our view on the content... */
	nsbeos_current_gc_set(view);

	/* render the content */
	thumbnail_redraw(content, big_width, big_height, &ctx);

	view->Sync();
	view->UnlockLooper();

	// restore the current gc
	nsbeos_current_gc_set(oldView);


	// now scale it down
//XXX: use Zeta's bilinear scaler ?
//#ifdef B_ZETA_VERSION
//	err = ScaleBitmap(*shot, *scaledBmp);
//#else
	thumbView->LockLooper();
	thumbView->DrawBitmap(big, big->Bounds(), small->Bounds());
	thumbView->Sync();
	thumbView->UnlockLooper();

	small->LockBits();
	thumbnail->LockBits();

	// copy it to the bitmap
	memcpy(thumbnail->Bits(), small->Bits(), thumbnail->BitsLength());

	thumbnail->UnlockBits();
	small->UnlockBits();

	/* register the thumbnail with the URL */
	if (url)
	  urldb_set_thumbnail(url, bitmap);

	bitmap_modified(bitmap);

	// cleanup
	small->RemoveChild(thumbView);
	delete thumbView;
	delete small;
	big->RemoveChild(view);
	delete view;
	delete big;

	return true;
}
