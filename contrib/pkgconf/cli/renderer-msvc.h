/*
 * renderer-msvc.h
 * MSVC library syntax renderer header
 *
 * Copyright (c) 2017 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef RENDERER_MSVC_H
#define RENDERER_MSVC_H

#include <libpkgconf/libpkgconf.h>

pkgconf_fragment_render_ops_t *msvc_renderer_get(void);

#endif
