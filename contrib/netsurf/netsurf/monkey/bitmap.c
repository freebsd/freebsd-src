/*
 * Copyright 2011 Daniel Silverstone <dsilvers@digital-scurf.org>
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

#include <stdbool.h>
#include <stdio.h>

#include "image/bitmap.h"

struct bitmap {
  void *ptr;
  size_t rowstride;
  int width;
  int height;
  unsigned int state;
};

void *bitmap_create(int width, int height, unsigned int state)
{
  struct bitmap *ret = calloc(sizeof(*ret), 1);
  if (ret == NULL)
    return NULL;
  
  ret->width = width;
  ret->height = height;
  ret->state = state;
  
  ret->ptr = calloc(width, height * 4);
  
  if (ret->ptr == NULL) {
    free(ret);
    return NULL;
  }
  
  return ret;
}

void bitmap_destroy(void *bitmap)
{
  struct bitmap *bmap = bitmap;
  free(bmap->ptr);
  free(bmap);
}

void bitmap_set_opaque(void *bitmap, bool opaque)
{
  struct bitmap *bmap = bitmap;
  
  if (opaque)
    bmap->state |= (BITMAP_OPAQUE);
  else
    bmap->state &= ~(BITMAP_OPAQUE);
}

bool bitmap_test_opaque(void *bitmap)
{
  return false;
}

bool bitmap_get_opaque(void *bitmap)
{
  struct bitmap *bmap = bitmap;
  
  return (bmap->state & BITMAP_OPAQUE) == BITMAP_OPAQUE;
}

unsigned char *bitmap_get_buffer(void *bitmap)
{
  struct bitmap *bmap = bitmap;
  
  return (unsigned char *)(bmap->ptr);
}

size_t bitmap_get_rowstride(void *bitmap)
{
  struct bitmap *bmap = bitmap;
  return bmap->width * 4;
}

size_t bitmap_get_bpp(void *bitmap)
{
  /* OMG?! */
  return 4;
}

bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
  return true;
}

void bitmap_modified(void *bitmap)
{
  struct bitmap *bmap = bitmap;
  bmap->state |= BITMAP_MODIFIED;
}

int bitmap_get_width(void *bitmap)
{
  struct bitmap *bmap = bitmap;
  return bmap->width;
}

int bitmap_get_height(void *bitmap)
{
  struct bitmap *bmap = bitmap;
  return bmap->height;
}
