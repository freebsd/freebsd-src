/*
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef HVECTOR_H
#define HVECTOR_H

#ifdef VECTOR_H
#undef VECTOR_H
#define __VECTOR_WAS_DEFINED
#endif

#define Allocator huge_allocator
#define vector huge_vector
#include <hugalloc.h>
#include <vector.h>

#undef VECTOR_H

#ifdef __VECTOR_WAS_DEFINED
#define VECTOR_H
#undef  __VECTOR_WAS_DEFINED
#endif

#undef Allocator
#undef vector

#endif
