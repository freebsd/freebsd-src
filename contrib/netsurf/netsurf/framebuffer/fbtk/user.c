/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit user widget.
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

#include <stdlib.h>
#include <stdbool.h>
#include <libnsfb.h>

#include "desktop/plotters.h"
#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"

#include "widget.h"

/* exported function documented in fbtk.h */
void *
fbtk_get_userpw(fbtk_widget_t *widget)
{
	if ((widget == NULL) ||
	    (widget->type != FB_WIDGET_TYPE_USER))
		return NULL;

	return widget->u.user.pw;
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_user(fbtk_widget_t *parent,
		 int x,
		 int y,
		 int width,
		 int height,
		 void *pw)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_USER, x, y, width, height);
	neww->u.user.pw = pw;
	neww->mapped = true;

	return neww;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
