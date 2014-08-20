/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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
 * Container format handling for themes etc. */

#ifndef __CONTAINER_H__
#define __CONTAINER_H__

#include <sys/types.h>

struct container_ctx;

/* reading interface */
struct container_ctx *container_open(const char *filename);
const unsigned char *container_get(struct container_ctx *ctx,
					const unsigned char *entryname,
					u_int32_t *size);
const unsigned char *container_get_name(struct container_ctx *ctx);
const unsigned char *container_get_author(struct container_ctx *ctx);
const unsigned char *container_iterate(struct container_ctx *ctx,
					int *state);

/* creating interface */
struct container_ctx *container_create(const char *filename,
					const unsigned char *name,
					const unsigned char *author);
void container_add(struct container_ctx *ctx, const unsigned char *entryname,
					const unsigned char *data,
					const u_int32_t datalen);

/* common interface */
void container_close(struct container_ctx *ctx);

#ifdef WITH_THEME_INSTALL
char *container_extract_theme(const char *themefile, const char *dirbasename);
#endif
#endif /* __CONTAINER_H__ */
