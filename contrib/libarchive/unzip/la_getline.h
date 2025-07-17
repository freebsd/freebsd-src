/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 */

#ifndef LA_GETLINE_H_INCLUDED
#define LA_GETLINE_H_INCLUDED

#include <stdio.h>
#ifndef HAVE_GETLINE
ssize_t getline(char **buf, size_t *bufsiz, FILE *fp);
#endif

#endif /* !LA_GETLINE_H_INCLUDED */
