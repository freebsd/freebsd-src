/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 */

#ifndef LAFE_GETLINE_H_INCLUDED
#define LAFE_GETLINE_H_INCLUDED

#include "lafe_platform.h"

#ifndef HAVE_GETLINE
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
ssize_t getline(char **buf, size_t *bufsiz, FILE *fp);
#endif

#endif /* !LAFE_GETLINE_H_INCLUDED */
