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

#ifndef FSET_H
#define FSET_H

#ifdef SET_H
#undef SET_H
#undef TREE_H
#define __SET_WAS_DEFINED
#endif

#define Allocator far_allocator
#define set far_set
#define rb_tree far_rb_tree
#include <faralloc.h>
#include <set.h>

#undef SET_H
#undef TREE_H

#ifdef __SET_WAS_DEFINED
#define SET_H
#define TREE_H
#undef  __SET_WAS_DEFINED
#endif

#undef Allocator
#undef set
#undef rb_tree

#endif
